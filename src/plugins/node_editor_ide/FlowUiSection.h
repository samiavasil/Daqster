#pragma once

#include <QHash>
#include <QJsonObject>

#include <vector>

#include <QtNodes/Definitions>

/**
 * Plain data structures describing the "ui" section of a .flow scene
 * (REQ-SW-PL-049). No Q_OBJECT, QtCore/QtWidgets only.
 *
 * The section captures the runtime layout of deembedded node widgets:
 * workspace geometry, per-node deembed state, geometry and autoStart flag.
 * Geometry uses the custom {x,y,w,h,maximized} format — NOT
 * QWidget::saveGeometry (unreliable for MDI sub-windows).
 */
namespace FlowUi {

struct Geometry
{
    int x = 0;
    int y = 0;
    int w = 0;
    int h = 0;
    bool maximized = false;

    QJsonObject toJson() const;
    static Geometry fromJson(const QJsonObject& json);
};

struct NodeUi
{
    bool deembedded = false;
    int workspace = 0;
    Geometry geometry;
    bool autoStart = false;

    QJsonObject toJson() const;
    static NodeUi fromJson(const QJsonObject& json);
};

struct WorkspaceUi
{
    int id = 0;
    Geometry geometry;
    bool tabbed = true;

    QJsonObject toJson() const;
    static WorkspaceUi fromJson(const QJsonObject& json);
};

struct UiSection
{
    int version = 1;
    std::vector<WorkspaceUi> workspaces;
    QHash<QtNodes::NodeId, NodeUi> nodes;

    QJsonObject toJson() const;
    static UiSection fromJson(const QJsonObject& json);
};

} // namespace FlowUi