#include "HeadlessEngine.h"

#include <QCoreApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QFile>
#include <QJsonParseError>
#include <QMessageBox>
#include <QDebug>

#include <QtNodes/NodeDelegateModel>
#include <QtNodes/NodeDelegateModelRegistry>
#include <QtNodes/DataFlowGraphModel>
#include <QtNodes/DataFlowGraphicsScene>
#include <QtNodes/BasicGraphicsScene>

#include <QPluginManager.h>
#include <INodeProvider.h>
#include <IStoppable.h>
#include <IStartable.h>
#include <logging/LogCategories.h>

namespace Daqster {

HeadlessEngine::HeadlessEngine(QObject* parent)
    : QObject(parent)
    , m_pluginManager(QPluginManager::instance())
{
}

HeadlessEngine::~HeadlessEngine()
{
    stopAllNodes();
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

    // 1. Register nodes (built-in + external plugins)
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

    // 4. Auto-start nodes
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

    // Register built-in nodes (same as RuntimeShell)
    // These are the basic nodes from node_editor_ide BuiltInNodes
    // For headless mode, we only register core models (no GUI widgets)
    // The actual registration of demo_nodeditor_nodes_core plugin nodes
    // happens via INodeProvider discovery below.

    // Discover and register external nodes via INodeProvider
    if (m_pluginManager) {
        QObjectList providers = m_pluginManager->instances(INodeProvider_IID);
        for (QObject* obj : providers) {
            auto* provider = qobject_cast<Daqster::INodeProvider*>(obj);
            if (!provider) continue;

            QString name = obj->property("name").toString();
            qCInfo(lcNodeEditor) << "HeadlessEngine: Discovered INodeProvider plugin:" << name;
            provider->registerNodes(*m_registry);
        }
    }

    return true;
}

bool HeadlessEngine::buildGraphModel()
{
    if (!m_registry)
        return false;

    m_graphModel = std::make_unique<QtNodes::DataFlowGraphModel>(m_registry);
    m_scene = std::make_unique<QtNodes::DataFlowGraphicsScene>(*m_graphModel, nullptr);

    // Connect scene signals if needed
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
        m_scene->clearScene();
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
        qCWarning(lcNodeEditor) << "HeadlessEngine: skipped unregistered node types:"
                                << skippedTypes.join(QStringLiteral(", "));
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