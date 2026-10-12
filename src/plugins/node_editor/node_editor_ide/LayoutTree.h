#pragma once

#include "FlowUiSection.h"

#include <QHash>
#include <QJsonObject>
#include <QString>

#include <optional>
#include <vector>

#include <QtNodes/Definitions>

/**
 * Plain data structures describing the schema-v2 "ui" section of a .flow
 * scene (REQ-SW-PL-053 Phase 2a). No Q_OBJECT, QtCore only — unit-testable
 * headless.
 *
 * v2 replaces the flat v1 workspace list with a recursive layout tree:
 * split/grid/tab containers whose leaves are node panels, plus a
 * workspace-level docks[] list. A node appearing anywhere in root or docks[]
 * IS deembedded; nodes not in the tree stay embedded in the canvas.
 *
 * Parsing is tolerant: unknown node types fall back to an empty panel,
 * missing fields take defaults, and a version != 2 section parses to an
 * empty LayoutSection (the caller keeps the v1 path).
 */
namespace FlowUi {

struct LayoutNode
{
    enum class Type { Split, Grid, Tab, Panel };
    enum class Orientation { Horizontal, Vertical };

    Type type = Type::Panel;

    // split
    Orientation orientation = Orientation::Horizontal;
    std::vector<LayoutNode> children;
    std::vector<int> sizes;

    // grid
    int rows = 0;
    int cols = 0;
    std::vector<std::optional<QtNodes::NodeId>> cells; // row-major; nullopt = empty cell

    // tab
    int currentIndex = 0;

    // panel
    QtNodes::NodeId nodeId = QtNodes::InvalidNodeId;
    QString title;

    QJsonObject toJson() const;
    static LayoutNode fromJson(const QJsonObject& json);
};

struct DockLayout
{
    enum class Area { Left, Right, Top, Bottom };

    QtNodes::NodeId node = QtNodes::InvalidNodeId;
    QString title;
    Area area = Area::Right;
    bool floating = false;
    Geometry geometry;

    QJsonObject toJson() const;
    static DockLayout fromJson(const QJsonObject& json);
};

struct WorkspaceLayout
{
    int id = 0;
    QString name;
    LayoutNode root;
    std::vector<DockLayout> docks;

    QJsonObject toJson() const;
    static WorkspaceLayout fromJson(const QJsonObject& json);
};

struct LayoutSection
{
    int version = 2;
    std::vector<WorkspaceLayout> workspaces;
    QHash<QtNodes::NodeId, NodeUi> nodes; // v2 keeps only autoStart; NodeUi reused for parse compat

    QJsonObject toJson() const;
    static LayoutSection fromJson(const QJsonObject& json);
};

} // namespace FlowUi