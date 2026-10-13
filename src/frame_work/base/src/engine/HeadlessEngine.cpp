#include "HeadlessEngine.h"

#include "ControlPlaneServer.h"
#include "NodeSchemaBuilder.h"

#include <QCoreApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QFile>
#include <QJsonParseError>
#include <QPointF>
#include <QDebug>

#include <QtNodes/NodeDelegateModel>
#include <QtNodes/NodeDelegateModelRegistry>
#include <QtNodes/DataFlowGraphModel>
#include <QtNodes/Definitions>

#include <QPluginManager.h>
#include <capabilities/INodeConfigSchema.h>
#include <capabilities/INodeProvider.h>
#include <capabilities/IStoppable.h>
#include <capabilities/IStartable.h>
#include <logging/LogCategories.h>

#include <algorithm>

namespace Daqster {

namespace {

/**
 * @brief The registered headless node types, sorted, ready for a message.
 *
 * Derived from the registry rather than hardcoded. Both error paths below used
 * to carry their own literal list of supported types, and both had to be edited
 * by hand every time registerNodesHeadless() grew — they did drift. A message
 * that can be silently wrong is worse than a shorter true one (INC-015).
 */
QStringList supportedHeadlessTypes(QtNodes::NodeDelegateModelRegistry const &registry)
{
    QStringList names;
    const auto &creators = registry.registeredModelCreators();
    names.reserve(static_cast<int>(creators.size()));
    for (auto const &entry : creators)
        names.append(entry.first);
    names.sort();
    return names;
}

/**
 * @brief The fail-fast message for flow types missing from the headless registry.
 */
QString unsupportedHeadlessMessage(QStringList const &types,
                                   QtNodes::NodeDelegateModelRegistry const &registry)
{
    const QStringList supported = supportedHeadlessTypes(registry);

    QString msg = QStringLiteral("Flow contains node types not supported in headless mode:\n  - ");
    msg += types.join(QStringLiteral("\n  - "));
    msg += QStringLiteral("\n\nRegistered headless node types (%1):\n  - ")
               .arg(supported.size());
    msg += supported.join(QStringLiteral(", "));
    msg += QStringLiteral("\n\nThese types are not registered for headless mode — either their "
                          "plugin is not loaded, or the type exists only in the GUI runtime. "
                          "'NodeRunner --run <file.flow>' runs the GUI runtime, which registers "
                          "every loaded type.");
    return msg;
}

} // namespace

HeadlessEngine::HeadlessEngine(QObject* parent)
    : QObject(parent)
    , m_pluginManager(QPluginManager::instance())
{
    // Nothing else consumes errorOccurred — verified 2026-10-08: no connect()
    // to this signal exists anywhere in src/. The rich fail-fast message
    // (unsupportedHeadlessMessage: the offending types plus the registered
    // set) was therefore emitted into the void, and the user only ever saw the
    // terse "flow validation failed during parse: <types>" line. AC2 asks for
    // a clear message; a message nobody prints is not one (INC-015).
    connect(this, &HeadlessEngine::errorOccurred, this,
            [this](const QString& message) {
                qCCritical(lcNodeEditor).noquote() << message;
            });
}

HeadlessEngine::~HeadlessEngine()
{
    // Tear the network endpoint down before the graph it points at.
    stopControlPlane();
    // stopAllNodes() is called explicitly by the caller (main.cpp)
    // to ensure graceful shutdown before graph model destruction.
}

bool HeadlessEngine::loadFlow(const QString& flowPath)
{
    if (flowPath.isEmpty()) {
        qCCritical(lcNodeEditor) << "HeadlessEngine: empty flow path";
        emit errorOccurred("Empty flow path");
        return false;
    }

    if (!QFile::exists(flowPath)) {
        qCCritical(lcNodeEditor) << "HeadlessEngine: flow file not found:" << flowPath;
        emit errorOccurred(QString("Flow file not found: %1").arg(flowPath));
        return false;
    }

    qCInfo(lcNodeEditor) << "HeadlessEngine: loading flow:" << flowPath;

    // 1. Register nodes (headless-compatible only)
    if (!registerNodes()) {
        emit errorOccurred("Failed to register node types");
        return false;
    }

    // 2. Build graph model
    if (!buildGraphModel()) {
        emit errorOccurred("Failed to build graph model");
        return false;
    }

    // 3. Parse flow file
    if (!parseFlowFile(flowPath)) {
        // Error already emitted by parseFlowFile
        return false;
    }

    // 4. Validate flow - fail fast on unsupported nodes
    if (!validateFlow()) {
        return false;
    }

    // 5. Auto-start nodes
    autoStartNodes();

    emit flowLoaded(true);
    qCInfo(lcNodeEditor) << "HeadlessEngine: flow loaded successfully";
    return true;
}

bool HeadlessEngine::reloadFlow(const QJsonObject& flow, QString* errorMessage)
{
    if (!m_graphModel) {
        // Nothing live yet: bring the registry/graph up first.
        if (!registerNodes()) {
            if (errorMessage) *errorMessage = QStringLiteral("failed to register node types");
            return false;
        }
        if (!buildGraphModel()) {
            if (errorMessage) *errorMessage = QStringLiteral("failed to build graph model");
            return false;
        }
    }

    qCInfo(lcNodeEditor) << "HeadlessEngine: hot-reload requested";

    // The old scene must stop while its graph is still valid.
    stopAllNodes();

    if (!buildGraphModel()) {
        if (errorMessage) *errorMessage = QStringLiteral("failed to rebuild graph model");
        return false;
    }
    if (!applySceneJson(flow)) {
        if (errorMessage) *errorMessage = QStringLiteral("invalid flow — see server log for details");
        return false;
    }
    if (!validateFlow()) {
        if (errorMessage) *errorMessage = QStringLiteral("flow validation failed (unsupported node types)");
        return false;
    }

    autoStartNodes();
    emit flowLoaded(true);
    qCInfo(lcNodeEditor) << "HeadlessEngine: hot-reload complete";
    return true;
}

void HeadlessEngine::stopAllNodes()
{
    if (!m_graphModel)
        return;

    qCInfo(lcNodeEditor) << "HeadlessEngine: stopping all nodes";

    for (const QtNodes::NodeId nodeId : m_graphModel->allNodeIds()) {
        auto* model = m_graphModel->delegateModel<QtNodes::NodeDelegateModel>(nodeId);
        if (!model)
            continue;

        auto* stoppable = dynamic_cast<Daqster::IStoppable*>(model);
        if (stoppable) {
            qCInfo(lcNodeEditor) << "HeadlessEngine: stopping node:" << model->caption() << "(id=" << nodeId << ")";
            stoppable->stop();
            m_runningNodes[nodeId] = false;
            emit nodeStopped(nodeId);
        }
    }
}

bool HeadlessEngine::registerNodes()
{
    m_registry = std::make_shared<QtNodes::NodeDelegateModelRegistry>();

    // Register nodes via INodeProvider discovery (plugins provide headless-compatible nodes only)
    if (m_pluginManager) {
        for (Daqster::INodeProvider* provider : m_pluginManager->nodeProviders()) {
            if (!provider) continue;

            qCInfo(lcNodeEditor) << "HeadlessEngine: Discovered INodeProvider plugin";
            provider->registerNodesHeadless(*m_registry);
        }
    }

    return true;
}

bool HeadlessEngine::buildGraphModel()
{
    if (!m_registry)
        return false;

    m_graphModel = std::make_unique<QtNodes::DataFlowGraphModel>(m_registry);

    return true;
}


bool HeadlessEngine::validateFlow()
{
    if (!m_registry || !m_graphModel) {
        qCCritical(lcNodeEditor) << "HeadlessEngine: registry or graph model not initialized";
        emit errorOccurred("Internal error: registry or graph model not initialized");
        return false;
    }

    QStringList unregisteredTypes;

    // Check all nodes in the graph model against the headless registry
    const auto& creators = m_registry->registeredModelCreators();
    const auto nodeIds = m_graphModel->allNodeIds();

    for (const QtNodes::NodeId nodeId : nodeIds) {
        auto* model = m_graphModel->delegateModel<QtNodes::NodeDelegateModel>(nodeId);
        if (!model)
            continue;

        const QString modelName = model->name();
        if (modelName.isEmpty()) {
            unregisteredTypes << QStringLiteral("<unnamed>");
            continue;
        }

        // Check if this model type is registered in the headless registry
        if (creators.find(modelName) == creators.end()) {
            unregisteredTypes << modelName;
        }
    }

    if (!unregisteredTypes.isEmpty()) {
        // Remove duplicates
        unregisteredTypes.removeDuplicates();

        const QString errorMsg = unsupportedHeadlessMessage(unregisteredTypes, *m_registry);

        qCCritical(lcNodeEditor) << "HeadlessEngine: flow validation failed:" << unregisteredTypes.join(", ");
        emit errorOccurred(errorMsg);
        return false;
    }

    return true;
}

bool HeadlessEngine::parseFlowFile(const QString& fileName)
{
    QFile file(fileName);
    if (!file.open(QIODevice::ReadOnly)) {
        qCWarning(lcNodeEditor) << "HeadlessEngine: cannot open" << fileName;
        emit errorOccurred(QString("Cannot open flow file: %1").arg(fileName));
        return false;
    }

    const QByteArray wholeFile = file.readAll();
    QJsonParseError parseError{};
    const QJsonDocument sceneDocument = QJsonDocument::fromJson(wholeFile, &parseError);
    if (parseError.error != QJsonParseError::NoError || !sceneDocument.isObject()) {
        qCWarning(lcNodeEditor) << "HeadlessEngine: invalid JSON in" << fileName
                                << ":" << parseError.errorString();
        emit errorOccurred(QString("Invalid JSON in flow file: %1").arg(parseError.errorString()));
        return false;
    }

    return applySceneJson(sceneDocument.object());
}

bool HeadlessEngine::applySceneJson(QJsonObject sceneJson)
{
    // The scene is a fresh live graph; clear any state from a previous scene.
    m_uiSection = UiSection{};
    m_runningNodes.clear();
    m_autoStartNodes.clear();

    // Extract the "ui" section BEFORE the node-cleaning loop
    if (!parseUiSection(sceneJson)) {
        qCWarning(lcNodeEditor) << "HeadlessEngine: failed to parse UI section";
        // Not fatal, continue without UI section
    }
    sceneJson.remove("ui");

    const QJsonArray nodesJsonArray = sceneJson["nodes"].toArray();

    auto* registry = m_registry.get();
    const auto& creators = registry->registeredModelCreators();

    QStringList skippedTypes;
    QSet<QtNodes::NodeId> skippedNodeIds;

    QJsonArray cleanedNodes;
    for (const auto& nodeValue : nodesJsonArray) {
        const QJsonObject nodeJson = nodeValue.toObject();
        const QString modelName = nodeJson["internal-data"].toObject()["model-name"].toString();
        if (modelName.isEmpty() || creators.count(modelName) == 0) {
            skippedTypes << (modelName.isEmpty() ? QStringLiteral("<unnamed>") : modelName);
            skippedNodeIds.insert(static_cast<QtNodes::NodeId>(nodeJson["id"].toInt()));
            continue;
        }
        cleanedNodes.append(nodeJson);
    }
    sceneJson["nodes"] = cleanedNodes;

    if (!skippedNodeIds.isEmpty()) {
        QJsonArray cleanedConnections;
        const QJsonArray connJsonArray = sceneJson["connections"].toArray();
        for (const auto& connValue : connJsonArray) {
            const QJsonObject connJson = connValue.toObject();
            const QtNodes::NodeId outNodeId =
                static_cast<QtNodes::NodeId>(connJson["outNodeId"].toInt());
            const QtNodes::NodeId inNodeId =
                static_cast<QtNodes::NodeId>(connJson["inNodeId"].toInt());
            if (skippedNodeIds.contains(outNodeId) || skippedNodeIds.contains(inNodeId))
                continue;
            cleanedConnections.append(connJson);
        }
        sceneJson["connections"] = cleanedConnections;
    }

    try {
        m_graphModel->load(sceneJson);
    } catch (const std::exception& e) {
        qCWarning(lcNodeEditor) << "HeadlessEngine: load failed:" << e.what();
        emit errorOccurred(QString("Failed to load flow: %1").arg(e.what()));
        return false;
    }

    // Store autoStart flags for later use
    m_autoStartNodes.clear();
    for (auto it = m_uiSection.nodes.constBegin(); it != m_uiSection.nodes.constEnd(); ++it) {
        m_autoStartNodes.insert(it.key(), it.value().autoStart);
    }

    const int loadedNodeCount = static_cast<int>(m_graphModel->allNodeIds().size());
    const int loadedConnCount = static_cast<int>(sceneJson["connections"].toArray().size());
    qCInfo(lcNodeEditor) << "HeadlessEngine: loaded nodes=" << loadedNodeCount
                         << "connections=" << loadedConnCount;

    if (!skippedTypes.isEmpty()) {
        skippedTypes.removeDuplicates();
        const QString errorMsg = unsupportedHeadlessMessage(skippedTypes, *m_registry);

        qCCritical(lcNodeEditor) << "HeadlessEngine: flow validation failed during parse:" << skippedTypes.join(", ");
        emit errorOccurred(errorMsg);
        return false;
    }

    return true;
}

bool HeadlessEngine::parseUiSection(const QJsonObject& sceneJson)
{
    const QJsonObject uiJson = sceneJson["ui"].toObject();
    if (uiJson.isEmpty())
        return true; // No UI section is valid (backward compatible)

    const int version = uiJson["version"].toInt(1);
    if (version > 1) {
        qWarning("HeadlessEngine: unsupported ui section version %d — ignoring ui section", version);
        return true;
    }
    m_uiSection.version = version;

    const QJsonArray workspacesJson = uiJson["workspaces"].toArray();
    for (const QJsonValue& value : workspacesJson) {
        WorkspaceUi ws;
        const QJsonObject wsJson = value.toObject();
        ws.id = wsJson["id"].toInt(ws.id);
        ws.tabbed = wsJson["tabbed"].toBool(ws.tabbed);
        const QJsonObject geomJson = wsJson["geometry"].toObject();
        ws.geometry.x = geomJson["x"].toInt(ws.geometry.x);
        ws.geometry.y = geomJson["y"].toInt(ws.geometry.y);
        ws.geometry.w = geomJson["w"].toInt(ws.geometry.w);
        ws.geometry.h = geomJson["h"].toInt(ws.geometry.h);
        ws.geometry.maximized = geomJson["maximized"].toBool(ws.geometry.maximized);
        m_uiSection.workspaces.push_back(ws);
    }

    const QJsonObject nodesJson = uiJson["nodes"].toObject();
    for (auto it = nodesJson.constBegin(); it != nodesJson.constEnd(); ++it) {
        bool ok = false;
        const QtNodes::NodeId nodeId =
            static_cast<QtNodes::NodeId>(it.key().toULongLong(&ok));
        if (!ok)
            continue;

        const QJsonObject nodeJson = it.value().toObject();
        NodeUi nui;
        nui.deembedded = nodeJson["deembedded"].toBool(nui.deembedded);
        nui.workspace = nodeJson["workspace"].toInt(nui.workspace);
        nui.autoStart = nodeJson["autoStart"].toBool(nui.autoStart);
        if (nui.deembedded) {
            const QJsonObject geomJson = nodeJson["geometry"].toObject();
            nui.geometry.x = geomJson["x"].toInt(nui.geometry.x);
            nui.geometry.y = geomJson["y"].toInt(nui.geometry.y);
            nui.geometry.w = geomJson["w"].toInt(nui.geometry.w);
            nui.geometry.h = geomJson["h"].toInt(nui.geometry.h);
            nui.geometry.maximized = geomJson["maximized"].toBool(nui.geometry.maximized);
        }
        m_uiSection.nodes.insert(nodeId, nui);
    }

    return true;
}

void HeadlessEngine::autoStartNodes()
{
    if (!m_graphModel)
        return;

    for (auto it = m_autoStartNodes.constBegin(); it != m_autoStartNodes.constEnd(); ++it) {
        const QtNodes::NodeId nodeId = it.key();
        const bool autoStart = it.value();

        if (!autoStart)
            continue;

        if (!m_graphModel->nodeExists(nodeId))
            continue;

        auto* model = m_graphModel->delegateModel<QtNodes::NodeDelegateModel>(nodeId);
        if (!model)
            continue;

        auto* startable = dynamic_cast<Daqster::IStartable*>(model);
        if (startable) {
            qCInfo(lcNodeEditor) << "HeadlessEngine: auto-starting node:" << model->caption() << "(id=" << nodeId << ")";
            startable->start();
            m_runningNodes[nodeId] = true;
            emit nodeStarted(nodeId);
        }
    }
}

QtNodes::NodeDelegateModel* HeadlessEngine::createNodeModel(const QString& modelName, const QJsonObject& nodeJson)
{
    Q_UNUSED(nodeJson);
    auto* registry = m_registry.get();
    const auto& creators = registry->registeredModelCreators();
    if (creators.count(modelName) == 0)
        return nullptr;

    return creators.at(modelName)().release();
}

// ═══════════════════════════════════════════════════════════════════════════
// Remote control plane (REQ-SW-PL-054)
// ═══════════════════════════════════════════════════════════════════════════

bool HeadlessEngine::startControlPlane(const ControlPlaneOptions& options, QString* errorMessage)
{
    if (m_controlPlane && m_controlPlane->isListening())
        return true;

    if (!m_controlPlane)
        m_controlPlane = std::make_unique<ControlPlaneServer>(this);

    // Stream lifecycle events to WebSocket subscribers (REQ-SW-PL-054 AC10).
    // connected once per freshly created server; the lambda target dies with it.
    connect(this, &HeadlessEngine::nodeStarted, m_controlPlane.get(),
            [this](QtNodes::NodeId id) {
                m_controlPlane->broadcastEvent(QStringLiteral("nodeStarted"),
                                               {{QStringLiteral("nodeId"), static_cast<qint64>(id)}});
            });
    connect(this, &HeadlessEngine::nodeStopped, m_controlPlane.get(),
            [this](QtNodes::NodeId id) {
                m_controlPlane->broadcastEvent(QStringLiteral("nodeStopped"),
                                               {{QStringLiteral("nodeId"), static_cast<qint64>(id)}});
            });
    connect(this, &HeadlessEngine::nodeConfigChanged, m_controlPlane.get(),
            [this](QtNodes::NodeId id) {
                m_controlPlane->broadcastEvent(QStringLiteral("nodeConfigChanged"),
                                               {{QStringLiteral("nodeId"), static_cast<qint64>(id)}});
            });
    connect(this, &HeadlessEngine::errorOccurred, m_controlPlane.get(),
            [this](const QString& message) {
                m_controlPlane->broadcastEvent(QStringLiteral("nodeError"),
                                               {{QStringLiteral("message"), message}});
            });
    // Graph mutations (REQ-SW-PL-063 AC13)
    connect(this, &HeadlessEngine::nodeCreated, m_controlPlane.get(),
            [this](QtNodes::NodeId id) {
                m_controlPlane->broadcastEvent(QStringLiteral("nodeCreated"),
                                               {{QStringLiteral("nodeId"), static_cast<qint64>(id)}});
            });
    connect(this, &HeadlessEngine::nodeDeleted, m_controlPlane.get(),
            [this](QtNodes::NodeId id) {
                m_controlPlane->broadcastEvent(QStringLiteral("nodeDeleted"),
                                               {{QStringLiteral("nodeId"), static_cast<qint64>(id)}});
            });
    connect(this, &HeadlessEngine::nodeMoved, m_controlPlane.get(),
            [this](QtNodes::NodeId id) {
                m_controlPlane->broadcastEvent(QStringLiteral("nodeMoved"),
                                               {{QStringLiteral("nodeId"), static_cast<qint64>(id)}});
            });
    connect(this, &HeadlessEngine::connectionCreated, m_controlPlane.get(),
            [this](quint64 outNodeId, int outPortIndex, quint64 inNodeId, int inPortIndex) {
                m_controlPlane->broadcastEvent(QStringLiteral("connectionCreated"),
                                               {{QStringLiteral("outNodeId"), static_cast<qint64>(outNodeId)},
                                                {QStringLiteral("outPortIndex"), outPortIndex},
                                                {QStringLiteral("inNodeId"), static_cast<qint64>(inNodeId)},
                                                {QStringLiteral("inPortIndex"), inPortIndex}});
            });
    connect(this, &HeadlessEngine::connectionDeleted, m_controlPlane.get(),
            [this](quint64 outNodeId, int outPortIndex, quint64 inNodeId, int inPortIndex) {
                m_controlPlane->broadcastEvent(QStringLiteral("connectionDeleted"),
                                               {{QStringLiteral("outNodeId"), static_cast<qint64>(outNodeId)},
                                                {QStringLiteral("outPortIndex"), outPortIndex},
                                                {QStringLiteral("inNodeId"), static_cast<qint64>(inNodeId)},
                                                {QStringLiteral("inPortIndex"), inPortIndex}});
            });

    QString error;
    if (!m_controlPlane->start(options, &error)) {
        if (errorMessage)
            *errorMessage = error;
        qCCritical(lcNodeEditor) << "HeadlessEngine: control plane failed to start:" << error;
        m_controlPlane.reset();
        return false;
    }
    return true;
}

void HeadlessEngine::stopControlPlane()
{
    if (!m_controlPlane)
        return;
    m_controlPlane->stop();
    m_controlPlane.reset();
}

bool HeadlessEngine::controlPlaneEnabled() const
{
    return m_controlPlane && m_controlPlane->isListening();
}

QVector<RuntimeController::NodeInfo> HeadlessEngine::nodes() const
{
    QVector<NodeInfo> result;
    if (!m_graphModel)
        return result;

    for (const QtNodes::NodeId nodeId : m_graphModel->allNodeIds()) {
        auto* model = m_graphModel->delegateModel<QtNodes::NodeDelegateModel>(nodeId);
        if (!model)
            continue;

        NodeInfo info;
        info.id = static_cast<quint64>(nodeId);
        info.type = model->name();
        info.name = model->caption();
        info.state = m_runningNodes.value(nodeId, false) ? QStringLiteral("running")
                                                         : QStringLiteral("stopped");
        result.append(info);
    }
    return result;
}

bool HeadlessEngine::nodeConfig(quint64 nodeId, QJsonObject& out) const
{
    if (!m_graphModel)
        return false;

    auto* model = m_graphModel->delegateModel<QtNodes::NodeDelegateModel>(
        static_cast<QtNodes::NodeId>(nodeId));
    if (!model)
        return false;

    out = model->save();
    return true;
}

bool HeadlessEngine::setNodeConfig(quint64 nodeId, const QJsonObject& config)
{
    if (!m_graphModel)
        return false;

    const QtNodes::NodeId id = static_cast<QtNodes::NodeId>(nodeId);
    auto* model = m_graphModel->delegateModel<QtNodes::NodeDelegateModel>(id);
    if (!model)
        return false;

    // NodeDelegateModel::load() returns void, so a malformed patch must not take
    // the whole engine down: snapshot first and roll back on a thrown error.
    const QJsonObject before = model->save();
    try {
        model->load(config);
    } catch (const std::exception& e) {
        qCWarning(lcNodeEditor) << "HeadlessEngine: node config rejected (id=" << id
                                << "):" << e.what();
        model->load(before);
        return false;
    }

    emit nodeConfigChanged(id);
    return true;
}

bool HeadlessEngine::startNode(quint64 nodeId)
{
    if (!m_graphModel)
        return false;

    const QtNodes::NodeId id = static_cast<QtNodes::NodeId>(nodeId);
    auto* model = m_graphModel->delegateModel<QtNodes::NodeDelegateModel>(id);
    if (!model)
        return false;

    auto* startable = dynamic_cast<Daqster::IStartable*>(model);
    if (!startable)
        return false;

    qCInfo(lcNodeEditor) << "HeadlessEngine: starting node via control plane:"
                         << model->caption() << "(id=" << id << ")";
    startable->start();
    m_runningNodes[id] = true;
    emit nodeStarted(id);
    return true;
}

bool HeadlessEngine::stopNode(quint64 nodeId)
{
    if (!m_graphModel)
        return false;

    const QtNodes::NodeId id = static_cast<QtNodes::NodeId>(nodeId);
    auto* model = m_graphModel->delegateModel<QtNodes::NodeDelegateModel>(id);
    if (!model)
        return false;

    auto* stoppable = dynamic_cast<Daqster::IStoppable*>(model);
    if (!stoppable)
        return false;

    qCInfo(lcNodeEditor) << "HeadlessEngine: stopping node via control plane:"
                         << model->caption() << "(id=" << id << ")";
    stoppable->stop();
    m_runningNodes[id] = false;
    emit nodeStopped(id);
    return true;
}

QJsonObject HeadlessEngine::flowJson() const
{
    QJsonObject flow;
    if (m_graphModel)
        flow = m_graphModel->save(); // { "nodes": [...], "connections": [...] }

    flow.insert(QStringLiteral("ui"), serializeUiSection());
    return flow;
}

QJsonObject HeadlessEngine::serializeUiSection() const
{
    QJsonObject ui;
    ui.insert(QStringLiteral("version"), m_uiSection.version);

    QJsonArray workspaces;
    for (const WorkspaceUi& ws : m_uiSection.workspaces) {
        QJsonObject wsJson;
        wsJson.insert(QStringLiteral("id"), ws.id);
        wsJson.insert(QStringLiteral("tabbed"), ws.tabbed);
        QJsonObject geometry;
        geometry.insert(QStringLiteral("x"), ws.geometry.x);
        geometry.insert(QStringLiteral("y"), ws.geometry.y);
        geometry.insert(QStringLiteral("w"), ws.geometry.w);
        geometry.insert(QStringLiteral("h"), ws.geometry.h);
        geometry.insert(QStringLiteral("maximized"), ws.geometry.maximized);
        wsJson.insert(QStringLiteral("geometry"), geometry);
        workspaces.append(wsJson);
    }
    ui.insert(QStringLiteral("workspaces"), workspaces);

    QJsonObject nodes;
    for (auto it = m_uiSection.nodes.constBegin(); it != m_uiSection.nodes.constEnd(); ++it) {
        const NodeUi& nui = it.value();
        QJsonObject nodeJson;
        nodeJson.insert(QStringLiteral("deembedded"), nui.deembedded);
        nodeJson.insert(QStringLiteral("workspace"), nui.workspace);
        nodeJson.insert(QStringLiteral("autoStart"), nui.autoStart);
        if (nui.deembedded) {
            QJsonObject geometry;
            geometry.insert(QStringLiteral("x"), nui.geometry.x);
            geometry.insert(QStringLiteral("y"), nui.geometry.y);
            geometry.insert(QStringLiteral("w"), nui.geometry.w);
            geometry.insert(QStringLiteral("h"), nui.geometry.h);
            geometry.insert(QStringLiteral("maximized"), nui.geometry.maximized);
            nodeJson.insert(QStringLiteral("geometry"), geometry);
        }
        nodes.insert(QString::number(it.key()), nodeJson);
    }
    ui.insert(QStringLiteral("nodes"), nodes);

    return ui;
}

QJsonObject HeadlessEngine::nodeSchema(quint64 nodeId) const
{
    if (!m_graphModel)
        return {};

    auto* model = m_graphModel->delegateModel<QtNodes::NodeDelegateModel>(
        static_cast<QtNodes::NodeId>(nodeId));
    if (!model)
        return {};

    return typeSchema(model->name());
}

QJsonObject HeadlessEngine::typeSchema(const QString& type) const
{
    if (!m_registry)
        return {};

    const auto& creators = m_registry->registeredModelCreators();
    const auto creatorIt = creators.find(type);
    if (creatorIt == creators.end())
        return {};

    const std::unique_ptr<QtNodes::NodeDelegateModel> fresh = creatorIt->second();
    if (!fresh)
        return {};

    // Enriched schema when the model self-describes (REQ-SW-PL-063 AC10);
    // best-effort derived from a throw-away save() otherwise.
    const auto* described = dynamic_cast<const Daqster::INodeConfigSchema*>(fresh.get());
    const QJsonObject provided = described ? described->configSchema() : QJsonObject();
    return nodeConfigSchema(fresh->save(), type, provided);
}

QVector<RuntimeController::NodeTypeInfo> HeadlessEngine::nodeTypes() const
{
    QVector<NodeTypeInfo> result;
    if (!m_registry)
        return result;

    const auto& creators = m_registry->registeredModelCreators();
    const auto& association = m_registry->registeredModelsCategoryAssociation();
    result.reserve(static_cast<int>(creators.size()));
    for (auto it = creators.begin(); it != creators.end(); ++it) {
        NodeTypeInfo info;
        info.type = it->first;
        const auto catIt = association.find(it->first);
        if (catIt != association.end())
            info.category = catIt->second;
        result.append(info);
    }
    std::sort(result.begin(), result.end(),
              [](const NodeTypeInfo& a, const NodeTypeInfo& b) {
                  if (a.category != b.category)
                      return a.category < b.category;
                  return a.type < b.type;
              });
    return result;
}

QVector<RuntimeController::ConnectionInfo> HeadlessEngine::connections() const
{
    QVector<ConnectionInfo> result;
    if (!m_graphModel)
        return result;

    const QJsonArray edges = m_graphModel->save()
                                 .value(QStringLiteral("connections")).toArray();
    result.reserve(edges.size());
    for (const QJsonValue& value : edges) {
        const QJsonObject c = value.toObject();
        ConnectionInfo info;
        info.outNodeId = static_cast<quint64>(c.value(QStringLiteral("outNodeId")).toInt());
        info.outPortIndex = c.value(QStringLiteral("outPortIndex")).toInt();
        info.inNodeId = static_cast<quint64>(c.value(QStringLiteral("inNodeId")).toInt());
        info.inPortIndex = c.value(QStringLiteral("inPortIndex")).toInt();
        result.append(info);
    }
    return result;
}

QJsonObject HeadlessEngine::nodeDetail(quint64 nodeId) const
{
    if (!m_graphModel)
        return {};

    const QtNodes::NodeId id = static_cast<QtNodes::NodeId>(nodeId);
    auto* model = m_graphModel->delegateModel<QtNodes::NodeDelegateModel>(id);
    if (!model)
        return {};

    QJsonObject ports;
    QJsonArray inPorts;
    for (unsigned int i = 0; i < model->nPorts(QtNodes::PortType::In); ++i) {
        inPorts.append(QJsonObject{{QStringLiteral("index"), static_cast<int>(i)},
                                   {QStringLiteral("caption"),
                                    model->portCaption(QtNodes::PortType::In,
                                                       static_cast<QtNodes::PortIndex>(i))}});
    }
    QJsonArray outPorts;
    for (unsigned int i = 0; i < model->nPorts(QtNodes::PortType::Out); ++i) {
        outPorts.append(QJsonObject{{QStringLiteral("index"), static_cast<int>(i)},
                                    {QStringLiteral("caption"),
                                     model->portCaption(QtNodes::PortType::Out,
                                                        static_cast<QtNodes::PortIndex>(i))}});
    }
    ports.insert(QStringLiteral("in"), inPorts);
    ports.insert(QStringLiteral("out"), outPorts);

    QJsonObject position;
    const QVariant pos = m_graphModel->nodeData(id, QtNodes::NodeRole::Position);
    if (pos.canConvert<QPointF>()) {
        const QPointF p = pos.toPointF();
        position.insert(QStringLiteral("x"), p.x());
        position.insert(QStringLiteral("y"), p.y());
    }

    QJsonObject out;
    out.insert(QStringLiteral("id"), static_cast<qint64>(id));
    out.insert(QStringLiteral("type"), model->name());
    out.insert(QStringLiteral("name"), model->caption());
    // The visible label lives on the graph model (NodeRole::Label), not on the
    // delegate model — model->label() is the type's default annotation.
    out.insert(QStringLiteral("label"),
               m_graphModel->nodeData(id, QtNodes::NodeRole::Label).toString());
    out.insert(QStringLiteral("labelVisible"),
               m_graphModel->nodeData(id, QtNodes::NodeRole::LabelVisible).toBool());
    out.insert(QStringLiteral("state"),
               m_runningNodes.value(id, false) ? QStringLiteral("running")
                                               : QStringLiteral("stopped"));
    out.insert(QStringLiteral("position"), position);
    out.insert(QStringLiteral("ports"), ports);
    out.insert(QStringLiteral("config"), model->save());
    out.insert(QStringLiteral("schema"), typeSchema(model->name()));
    return out;
}

quint64 HeadlessEngine::createNode(const QString& type, double x, double y,
                                   const QJsonObject& config, QString* errorMessage)
{
    if (!m_graphModel || !m_registry) {
        if (errorMessage)
            *errorMessage = QStringLiteral("graph is not ready");
        return 0;
    }
    if (!m_registry->registeredModelCreators().count(type)) {
        if (errorMessage)
            *errorMessage = QStringLiteral("unknown node type: %1").arg(type);
        return 0;
    }

    const QtNodes::NodeId id = m_graphModel->addNode(type);
    m_graphModel->setNodeData(id, QtNodes::NodeRole::Position, QPointF(x, y));

    if (!config.isEmpty()) {
        if (auto* model = m_graphModel->delegateModel<QtNodes::NodeDelegateModel>(id))
            model->load(config);
    }

    emit nodeCreated(id);
    return static_cast<quint64>(id);
}

bool HeadlessEngine::removeNode(quint64 nodeId, QString* errorMessage)
{
    if (!m_graphModel) {
        if (errorMessage)
            *errorMessage = QStringLiteral("graph is not ready");
        return false;
    }

    const QtNodes::NodeId id = static_cast<QtNodes::NodeId>(nodeId);
    if (!m_graphModel->delegateModel<QtNodes::NodeDelegateModel>(id)) {
        if (errorMessage)
            *errorMessage = QStringLiteral("node not found");
        return false;
    }

    // deleteNode() also removes every connection attached to the node, but the
    // control plane cannot see which ones: capture them first for the event.
    const QVector<ConnectionInfo> edges = connections();
    m_graphModel->deleteNode(id);
    m_runningNodes.remove(id);

    for (const ConnectionInfo& c : edges) {
        if (c.outNodeId == nodeId || c.inNodeId == nodeId)
            emit connectionDeleted(c.outNodeId, c.outPortIndex, c.inNodeId, c.inPortIndex);
    }
    emit nodeDeleted(id);
    return true;
}

bool HeadlessEngine::setNodePosition(quint64 nodeId, double x, double y)
{
    if (!m_graphModel)
        return false;
    const QtNodes::NodeId id = static_cast<QtNodes::NodeId>(nodeId);
    if (!m_graphModel->delegateModel<QtNodes::NodeDelegateModel>(id))
        return false;
    m_graphModel->setNodeData(id, QtNodes::NodeRole::Position, QPointF(x, y));
    emit nodeMoved(id);
    return true;
}

bool HeadlessEngine::setNodeLabel(quint64 nodeId, const QString& label, bool visible)
{
    if (!m_graphModel)
        return false;
    const QtNodes::NodeId id = static_cast<QtNodes::NodeId>(nodeId);
    if (!m_graphModel->delegateModel<QtNodes::NodeDelegateModel>(id))
        return false;
    m_graphModel->setNodeData(id, QtNodes::NodeRole::Label, label);
    m_graphModel->setNodeData(id, QtNodes::NodeRole::LabelVisible, visible);
    emit nodeMoved(id); // label change is a node metadata change
    return true;
}

bool HeadlessEngine::addConnection(const ConnectionInfo& connection, QString* errorMessage)
{
    if (!m_graphModel) {
        if (errorMessage)
            *errorMessage = QStringLiteral("graph is not ready");
        return false;
    }

    const QtNodes::ConnectionId c{
        static_cast<QtNodes::NodeId>(connection.outNodeId),
        static_cast<QtNodes::PortIndex>(connection.outPortIndex),
        static_cast<QtNodes::NodeId>(connection.inNodeId),
        static_cast<QtNodes::PortIndex>(connection.inPortIndex)};

    if (!m_graphModel->connectionPossible(c)) {
        if (errorMessage)
            *errorMessage = QStringLiteral("connection not possible");
        return false;
    }
    m_graphModel->addConnection(c);
    emit connectionCreated(connection.outNodeId, connection.outPortIndex,
                           connection.inNodeId, connection.inPortIndex);
    return true;
}

bool HeadlessEngine::removeConnection(const ConnectionInfo& connection)
{
    if (!m_graphModel)
        return false;

    const QtNodes::ConnectionId c{
        static_cast<QtNodes::NodeId>(connection.outNodeId),
        static_cast<QtNodes::PortIndex>(connection.outPortIndex),
        static_cast<QtNodes::NodeId>(connection.inNodeId),
        static_cast<QtNodes::PortIndex>(connection.inPortIndex)};

    if (!m_graphModel->deleteConnection(c))
        return false;
    emit connectionDeleted(connection.outNodeId, connection.outPortIndex,
                           connection.inNodeId, connection.inPortIndex);
    return true;
}

bool HeadlessEngine::saveFlow(const QString& path, QString* errorMessage)
{
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        if (errorMessage)
            *errorMessage = QStringLiteral("cannot open %1 for writing").arg(path);
        return false;
    }
    if (file.write(QJsonDocument(flowJson()).toJson(QJsonDocument::Indented)) < 0) {
        if (errorMessage)
            *errorMessage = QStringLiteral("cannot write %1").arg(path);
        file.close();
        return false;
    }
    file.close();
    return true;
}

} // namespace Daqster