#include "LayoutTree.h"

#include <QJsonArray>
#include <QJsonValue>
#include <QString>
#include <QtGlobal>

namespace FlowUi {

namespace {

QString typeToString(LayoutNode::Type type)
{
    switch (type) {
    case LayoutNode::Type::Split: return QStringLiteral("split");
    case LayoutNode::Type::Grid:  return QStringLiteral("grid");
    case LayoutNode::Type::Tab:   return QStringLiteral("tab");
    case LayoutNode::Type::Panel: return QStringLiteral("panel");
    }
    return QStringLiteral("panel");
}

LayoutNode::Type typeFromString(const QString& type)
{
    if (type == QLatin1String("split")) return LayoutNode::Type::Split;
    if (type == QLatin1String("grid"))  return LayoutNode::Type::Grid;
    if (type == QLatin1String("tab"))   return LayoutNode::Type::Tab;
    return LayoutNode::Type::Panel; // unknown type → panel with no node
}

QString orientationToString(LayoutNode::Orientation orientation)
{
    return orientation == LayoutNode::Orientation::Vertical
        ? QStringLiteral("vertical")
        : QStringLiteral("horizontal");
}

LayoutNode::Orientation orientationFromString(const QString& orientation)
{
    return orientation == QLatin1String("vertical")
        ? LayoutNode::Orientation::Vertical
        : LayoutNode::Orientation::Horizontal;
}

QString areaToString(DockLayout::Area area)
{
    switch (area) {
    case DockLayout::Area::Left:   return QStringLiteral("left");
    case DockLayout::Area::Right:  return QStringLiteral("right");
    case DockLayout::Area::Top:    return QStringLiteral("top");
    case DockLayout::Area::Bottom: return QStringLiteral("bottom");
    }
    return QStringLiteral("right");
}

DockLayout::Area areaFromString(const QString& area)
{
    if (area == QLatin1String("left"))   return DockLayout::Area::Left;
    if (area == QLatin1String("top"))    return DockLayout::Area::Top;
    if (area == QLatin1String("bottom")) return DockLayout::Area::Bottom;
    return DockLayout::Area::Right; // unknown → right
}

} // namespace

// ── LayoutNode ──────────────────────────────────────────────────────────────

QJsonObject LayoutNode::toJson() const
{
    QJsonObject json;
    json[QStringLiteral("type")] = typeToString(type);

    switch (type) {
    case Type::Split: {
        json[QStringLiteral("orientation")] = orientationToString(orientation);
        QJsonArray childrenJson;
        for (const LayoutNode& child : children)
            childrenJson.append(child.toJson());
        json[QStringLiteral("children")] = childrenJson;
        if (!sizes.empty()) {
            QJsonArray sizesJson;
            for (int size : sizes)
                sizesJson.append(size);
            json[QStringLiteral("sizes")] = sizesJson;
        }
        break;
    }
    case Type::Grid: {
        json[QStringLiteral("rows")] = rows;
        json[QStringLiteral("cols")] = cols;
        QJsonArray cellsJson;
        for (const auto& cell : cells) {
            if (cell.has_value())
                cellsJson.append(QJsonObject{{QStringLiteral("node"), static_cast<double>(*cell)}});
            else
                cellsJson.append(QJsonValue::Null);
        }
        json[QStringLiteral("cells")] = cellsJson;
        break;
    }
    case Type::Tab: {
        QJsonArray childrenJson;
        for (const LayoutNode& child : children)
            childrenJson.append(child.toJson());
        json[QStringLiteral("children")] = childrenJson;
        json[QStringLiteral("current")] = currentIndex;
        break;
    }
    case Type::Panel: {
        if (nodeId != QtNodes::InvalidNodeId)
            json[QStringLiteral("node")] = static_cast<double>(nodeId);
        if (!title.isEmpty())
            json[QStringLiteral("title")] = title;
        break;
    }
    }
    return json;
}

LayoutNode LayoutNode::fromJson(const QJsonObject& json)
{
    LayoutNode node;
    node.type = typeFromString(json[QStringLiteral("type")].toString());

    switch (node.type) {
    case Type::Split: {
        node.orientation =
            orientationFromString(json[QStringLiteral("orientation")].toString());
        const QJsonArray childrenJson = json[QStringLiteral("children")].toArray();
        for (const QJsonValue& value : childrenJson)
            node.children.push_back(LayoutNode::fromJson(value.toObject()));
        const QJsonArray sizesJson = json[QStringLiteral("sizes")].toArray();
        for (const QJsonValue& value : sizesJson)
            node.sizes.push_back(value.toInt());
        break;
    }
    case Type::Grid: {
        node.rows = json[QStringLiteral("rows")].toInt(0);
        node.cols = json[QStringLiteral("cols")].toInt(0);
        const QJsonArray cellsJson = json[QStringLiteral("cells")].toArray();
        for (const QJsonValue& value : cellsJson) {
            if (value.isNull()) {
                node.cells.push_back(std::nullopt);
            } else {
                const QJsonObject cellJson = value.toObject();
                const int cellNode = cellJson[QStringLiteral("node")].toInt(-1);
                if (cellNode >= 0)
                    node.cells.push_back(static_cast<QtNodes::NodeId>(cellNode));
                else
                    node.cells.push_back(std::nullopt);
            }
        }
        break;
    }
    case Type::Tab: {
        const QJsonArray childrenJson = json[QStringLiteral("children")].toArray();
        for (const QJsonValue& value : childrenJson)
            node.children.push_back(LayoutNode::fromJson(value.toObject()));
        node.currentIndex = json[QStringLiteral("current")].toInt(0);
        break;
    }
    case Type::Panel: {
        node.nodeId = static_cast<QtNodes::NodeId>(json[QStringLiteral("node")].toInt(-1));
        node.title = json[QStringLiteral("title")].toString();
        break;
    }
    }
    return node;
}

// ── DockLayout ──────────────────────────────────────────────────────────────

QJsonObject DockLayout::toJson() const
{
    QJsonObject json;
    json[QStringLiteral("node")] = static_cast<double>(node);
    if (!title.isEmpty())
        json[QStringLiteral("title")] = title;
    json[QStringLiteral("area")] = areaToString(area);
    json[QStringLiteral("floating")] = floating;
    json[QStringLiteral("geometry")] = geometry.toJson();
    return json;
}

DockLayout DockLayout::fromJson(const QJsonObject& json)
{
    DockLayout dock;
    dock.node = static_cast<QtNodes::NodeId>(json[QStringLiteral("node")].toInt(-1));
    dock.title = json[QStringLiteral("title")].toString();
    dock.area = areaFromString(json[QStringLiteral("area")].toString());
    dock.floating = json[QStringLiteral("floating")].toBool(false);
    dock.geometry = Geometry::fromJson(json[QStringLiteral("geometry")].toObject());
    return dock;
}

// ── WorkspaceLayout ─────────────────────────────────────────────────────────

QJsonObject WorkspaceLayout::toJson() const
{
    QJsonObject json;
    json[QStringLiteral("id")] = id;
    if (!name.isEmpty())
        json[QStringLiteral("name")] = name;
    json[QStringLiteral("root")] = root.toJson();

    QJsonArray docksJson;
    for (const DockLayout& dock : docks)
        docksJson.append(dock.toJson());
    json[QStringLiteral("docks")] = docksJson;
    return json;
}

WorkspaceLayout WorkspaceLayout::fromJson(const QJsonObject& json)
{
    WorkspaceLayout ws;
    ws.id = json[QStringLiteral("id")].toInt(0);
    ws.name = json[QStringLiteral("name")].toString();
    ws.root = LayoutNode::fromJson(json[QStringLiteral("root")].toObject());

    const QJsonArray docksJson = json[QStringLiteral("docks")].toArray();
    for (const QJsonValue& value : docksJson)
        ws.docks.push_back(DockLayout::fromJson(value.toObject()));
    return ws;
}

// ── LayoutSection ───────────────────────────────────────────────────────────

QJsonObject LayoutSection::toJson() const
{
    QJsonObject json;
    json[QStringLiteral("version")] = version;

    QJsonArray workspacesJson;
    for (const WorkspaceLayout& ws : workspaces)
        workspacesJson.append(ws.toJson());
    json[QStringLiteral("workspaces")] = workspacesJson;

    QJsonObject nodesJson;
    for (auto it = nodes.constBegin(); it != nodes.constEnd(); ++it)
        nodesJson[QString::number(it.key())] = it.value().toJson();
    json[QStringLiteral("nodes")] = nodesJson;

    return json;
}

LayoutSection LayoutSection::fromJson(const QJsonObject& json)
{
    LayoutSection section;

    const int version = json[QStringLiteral("version")].toInt(2);
    if (version != 2) {
        qWarning("LayoutSection: unsupported ui section version %d — ignoring ui section",
                 version);
        return section; // empty section
    }
    section.version = version;

    const QJsonArray workspacesJson = json[QStringLiteral("workspaces")].toArray();
    for (const QJsonValue& value : workspacesJson)
        section.workspaces.push_back(WorkspaceLayout::fromJson(value.toObject()));

    const QJsonObject nodesJson = json[QStringLiteral("nodes")].toObject();
    for (auto it = nodesJson.constBegin(); it != nodesJson.constEnd(); ++it) {
        bool ok = false;
        const QtNodes::NodeId nodeId =
            static_cast<QtNodes::NodeId>(it.key().toULongLong(&ok));
        if (!ok)
            continue; // skip non-numeric node keys
        section.nodes.insert(nodeId, NodeUi::fromJson(it.value().toObject()));
    }

    return section;
}

} // namespace FlowUi