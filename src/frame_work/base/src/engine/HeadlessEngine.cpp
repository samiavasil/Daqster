#include "HeadlessEngine.h"

#include <QCoreApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QFile>
#include <QJsonParseError>
#include <QDebug>

#include <QtNodes/NodeDelegateModel>
#include <QtNodes/NodeDelegateModelRegistry>
#include <QtNodes/DataFlowGraphModel>

#include <QPluginManager.h>
#include <capabilities/INodeProvider.h>
#include <capabilities/IStoppable.h>
#include <capabilities/IStartable.h>
#include <logging/LogCategories.h>

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

    QJsonObject sceneJson = sceneDocument.object();

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

} // namespace Daqster