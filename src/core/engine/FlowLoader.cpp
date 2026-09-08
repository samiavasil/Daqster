#include "FlowLoader.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonParseError>
#include <QFile>
#include <QDebug>

#include <QtNodes/NodeDelegateModel>
#include <QtNodes/NodeDelegateModelRegistry>
#include <QtNodes/DataFlowGraphModel>
#include <QtNodes/DataFlowGraphicsScene>

#include <logging/LogCategories.h>

namespace Daqster {

FlowLoader::FlowLoader(QtNodes::NodeDelegateModelRegistry* registry)
    : m_registry(registry)
{
}

bool FlowLoader::loadFlow(const QJsonObject& sceneJson,
                          QtNodes::DataFlowGraphModel* graphModel,
                          QStringList* skippedTypes)
{
    if (!m_registry || !graphModel) {
        qCWarning(lcNodeEditor) << "FlowLoader: null registry or graphModel";
        return false;
    }

    QJsonObject json = sceneJson; // Make a copy since we modify it

    // Extract UI section if present
    UiSection uiSection;
    parseUiSection(json, &uiSection);
    json.remove("ui");

    const QJsonArray nodesJsonArray = json["nodes"].toArray();

    const auto& creators = m_registry->registeredModelCreators();

    QStringList localSkippedTypes;
    QSet<QtNodes::NodeId> skippedNodeIds;

    QJsonArray cleanedNodes;
    if (!parseNodes(nodesJsonArray, cleanedNodes, skippedNodeIds, &localSkippedTypes)) {
        return false;
    }
    json["nodes"] = cleanedNodes;

    if (!skippedNodeIds.isEmpty()) {
        QJsonArray cleanedConnections;
        const QJsonArray connJsonArray = json["connections"].toArray();
        if (!parseConnections(connJsonArray, skippedNodeIds, cleanedConnections)) {
            return false;
        }
        json["connections"] = cleanedConnections;
    }

    try {
        // We need a scene to load into the graph model
        // Create a temporary scene if needed
        QtNodes::DataFlowGraphicsScene tempScene(*graphModel, nullptr);
        tempScene.clearScene();
        graphModel->load(json);
    } catch (const std::exception& e) {
        qCWarning(lcNodeEditor) << "FlowLoader: load failed:" << e.what();
        return false;
    }

    if (skippedTypes) {
        *skippedTypes = localSkippedTypes;
    }

    if (!localSkippedTypes.isEmpty()) {
        qCWarning(lcNodeEditor) << "FlowLoader: skipped unregistered node types:"
                                << localSkippedTypes.join(QStringLiteral(", "));
    }

    return true;
}

bool FlowLoader::loadFlowFromFile(const QString& fileName,
                                  QtNodes::DataFlowGraphModel* graphModel,
                                  QStringList* skippedTypes,
                                  UiSection* uiSection)
{
    QFile file(fileName);
    if (!file.open(QIODevice::ReadOnly)) {
        qCWarning(lcNodeEditor) << "FlowLoader: cannot open" << fileName;
        return false;
    }

    const QByteArray wholeFile = file.readAll();
    QJsonParseError parseError{};
    const QJsonDocument sceneDocument = QJsonDocument::fromJson(wholeFile, &parseError);
    if (parseError.error != QJsonParseError::NoError || !sceneDocument.isObject()) {
        qCWarning(lcNodeEditor) << "FlowLoader: invalid JSON in" << fileName
                                << ":" << parseError.errorString();
        return false;
    }

    QJsonObject sceneJson = sceneDocument.object();

    // Parse UI section first (before modifying the JSON)
    if (uiSection) {
        parseUiSection(sceneJson, uiSection);
    }

    return loadFlow(sceneJson, graphModel, skippedTypes);
}

bool FlowLoader::parseNodes(const QJsonArray& nodesJsonArray,
                            QJsonArray& cleanedNodes,
                            QSet<QtNodes::NodeId>& skippedNodeIds,
                            QStringList* skippedTypes)
{
    const auto& creators = m_registry->registeredModelCreators();

    for (const auto& nodeValue : nodesJsonArray) {
        const QJsonObject nodeJson = nodeValue.toObject();
        const QString modelName = nodeJson["internal-data"].toObject()["model-name"].toString();
        if (modelName.isEmpty() || creators.count(modelName) == 0) {
            skippedNodeIds.insert(static_cast<QtNodes::NodeId>(nodeJson["id"].toInt()));
            if (skippedTypes) {
                skippedTypes->append(modelName.isEmpty() ? QStringLiteral("<unnamed>") : modelName);
            }
            continue;
        }
        cleanedNodes.append(nodeJson);
    }

    return true;
}

bool FlowLoader::parseConnections(const QJsonArray& connJsonArray,
                                  const QSet<QtNodes::NodeId>& skippedNodeIds,
                                  QJsonArray& cleanedConnections)
{
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

    return true;
}

bool FlowLoader::parseUiSection(const QJsonObject& sceneJson, UiSection* uiSection)
{
    if (!uiSection)
        return true;

    const QJsonObject uiJson = sceneJson["ui"].toObject();
    if (uiJson.isEmpty())
        return true;

    const int version = uiJson["version"].toInt(1);
    if (version > 1) {
        qWarning("FlowLoader: unsupported ui section version %d — ignoring ui section", version);
        return true;
    }
    uiSection->version = version;

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
        uiSection->workspaces.push_back(ws);
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
        uiSection->nodes.insert(nodeId, nui);
    }

    return true;
}

// ── Geometry ────────────────────────────────────────────────────────────────

QJsonObject FlowLoader::Geometry::toJson() const
{
    QJsonObject json;
    json[QStringLiteral("x")] = x;
    json[QStringLiteral("y")] = y;
    json[QStringLiteral("w")] = w;
    json[QStringLiteral("h")] = h;
    json[QStringLiteral("maximized")] = maximized;
    return json;
}

FlowLoader::Geometry FlowLoader::Geometry::fromJson(const QJsonObject& json)
{
    Geometry g;
    g.x = json[QStringLiteral("x")].toInt(g.x);
    g.y = json[QStringLiteral("y")].toInt(g.y);
    g.w = json[QStringLiteral("w")].toInt(g.w);
    g.h = json[QStringLiteral("h")].toInt(g.h);
    g.maximized = json[QStringLiteral("maximized")].toBool(g.maximized);
    return g;
}

// ── NodeUi ──────────────────────────────────────────────────────────────────

QJsonObject FlowLoader::NodeUi::toJson() const
{
    QJsonObject json;
    json[QStringLiteral("deembedded")] = deembedded;
    json[QStringLiteral("workspace")] = workspace;
    if (deembedded) {
        json[QStringLiteral("geometry")] = geometry.toJson();
    }
    json[QStringLiteral("autoStart")] = autoStart;
    return json;
}

FlowLoader::NodeUi FlowLoader::NodeUi::fromJson(const QJsonObject& json)
{
    NodeUi n;
    n.deembedded = json[QStringLiteral("deembedded")].toBool(n.deembedded);
    n.workspace = json[QStringLiteral("workspace")].toInt(n.workspace);
    if (n.deembedded) {
        n.geometry = Geometry::fromJson(json[QStringLiteral("geometry")].toObject());
    }
    n.autoStart = json[QStringLiteral("autoStart")].toBool(n.autoStart);
    return n;
}

// ── WorkspaceUi ─────────────────────────────────────────────────────────────

QJsonObject FlowLoader::WorkspaceUi::toJson() const
{
    QJsonObject json;
    json[QStringLiteral("id")] = id;
    json[QStringLiteral("geometry")] = geometry.toJson();
    json[QStringLiteral("tabbed")] = tabbed;
    return json;
}

FlowLoader::WorkspaceUi FlowLoader::WorkspaceUi::fromJson(const QJsonObject& json)
{
    WorkspaceUi w;
    w.id = json[QStringLiteral("id")].toInt(w.id);
    w.geometry = Geometry::fromJson(json[QStringLiteral("geometry")].toObject());
    w.tabbed = json[QStringLiteral("tabbed")].toBool(w.tabbed);
    return w;
}

// ── UiSection ───────────────────────────────────────────────────────────────

QJsonObject FlowLoader::UiSection::toJson() const
{
    QJsonObject json;
    json[QStringLiteral("version")] = version;

    QJsonArray workspacesJson;
    for (const WorkspaceUi& ws : workspaces) {
        workspacesJson.append(ws.toJson());
    }
    json[QStringLiteral("workspaces")] = workspacesJson;

    QJsonObject nodesJson;
    for (auto it = nodes.constBegin(); it != nodes.constEnd(); ++it) {
        nodesJson[QString::number(it.key())] = it.value().toJson();
    }
    json[QStringLiteral("nodes")] = nodesJson;

    return json;
}

FlowLoader::UiSection FlowLoader::UiSection::fromJson(const QJsonObject& json)
{
    UiSection ui;

    const int version = json[QStringLiteral("version")].toInt(1);
    if (version > 1) {
        qWarning("FlowLoader: unsupported ui section version %d — ignoring ui section", version);
        return ui;
    }
    ui.version = version;

    const QJsonArray workspacesJson = json[QStringLiteral("workspaces")].toArray();
    for (const QJsonValue& value : workspacesJson) {
        ui.workspaces.push_back(WorkspaceUi::fromJson(value.toObject()));
    }

    const QJsonObject nodesJson = json[QStringLiteral("nodes")].toObject();
    for (auto it = nodesJson.constBegin(); it != nodesJson.constEnd(); ++it) {
        bool ok = false;
        const QtNodes::NodeId nodeId =
            static_cast<QtNodes::NodeId>(it.key().toULongLong(&ok));
        if (!ok)
            continue;
        ui.nodes.insert(nodeId, NodeUi::fromJson(it.value().toObject()));
    }

    return ui;
}

} // namespace Daqster