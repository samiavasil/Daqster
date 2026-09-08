#include "FlowUiSection.h"

#include <QJsonArray>
#include <QJsonValue>
#include <QString>
#include <QtGlobal>

namespace FlowUi {

// ── Geometry ────────────────────────────────────────────────────────────────

QJsonObject Geometry::toJson() const
{
    QJsonObject json;
    json[QStringLiteral("x")] = x;
    json[QStringLiteral("y")] = y;
    json[QStringLiteral("w")] = w;
    json[QStringLiteral("h")] = h;
    json[QStringLiteral("maximized")] = maximized;
    return json;
}

Geometry Geometry::fromJson(const QJsonObject& json)
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

QJsonObject NodeUi::toJson() const
{
    QJsonObject json;
    json[QStringLiteral("deembedded")] = deembedded;
    json[QStringLiteral("workspace")] = workspace;
    // Geometry key is emitted ONLY when the node is deembedded.
    if (deembedded) {
        json[QStringLiteral("geometry")] = geometry.toJson();
    }
    json[QStringLiteral("autoStart")] = autoStart;
    return json;
}

NodeUi NodeUi::fromJson(const QJsonObject& json)
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

QJsonObject WorkspaceUi::toJson() const
{
    QJsonObject json;
    json[QStringLiteral("id")] = id;
    json[QStringLiteral("geometry")] = geometry.toJson();
    json[QStringLiteral("tabbed")] = tabbed;
    return json;
}

WorkspaceUi WorkspaceUi::fromJson(const QJsonObject& json)
{
    WorkspaceUi w;
    w.id = json[QStringLiteral("id")].toInt(w.id);
    w.geometry = Geometry::fromJson(json[QStringLiteral("geometry")].toObject());
    w.tabbed = json[QStringLiteral("tabbed")].toBool(w.tabbed);
    return w;
}

// ── UiSection ───────────────────────────────────────────────────────────────

QJsonObject UiSection::toJson() const
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

UiSection UiSection::fromJson(const QJsonObject& json)
{
    UiSection ui;

    const int version = json[QStringLiteral("version")].toInt(1);
    if (version > 1) {
        qWarning("FlowUiSection: unsupported ui section version %d — ignoring ui section",
                 version);
        return ui; // empty section, current behavior for old flows
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
            continue; // skip non-numeric node keys
        ui.nodes.insert(nodeId, NodeUi::fromJson(it.value().toObject()));
    }

    return ui;
}

} // namespace FlowUi