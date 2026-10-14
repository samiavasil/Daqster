#pragma once

#include "LayoutTree.h"

#include <QMainWindow>

#include <functional>

/**
 * Builds a runtime workspace from a FlowUi::WorkspaceLayout tree and captures
 * the live widget hierarchy back into a WorkspaceLayout (REQ-SW-PL-053 Phase 2a).
 *
 * buildWorkspace() maps each layout node to a Qt container:
 *   split → QSplitter, grid → QWidget+QGridLayout, tab → QTabWidget,
 *   panel → the node's embeddedWidget() from the provider, dock → QDockWidget.
 *
 * Every widget is tagged with setProperty("layoutNodeType") (and
 * "layoutNodeId" for panels) so captureWorkspace() can walk the hierarchy and
 * reconstruct the tree. A provider returning nullptr skips the node/cell.
 *
 * This class touches QtWidgets; the LayoutTree data model it consumes is
 * QtCore-only and can be unit-tested headless.
 */
namespace FlowUi {

class LayoutEngine
{
public:
    /// Returns the widget for a node, or nullptr if the node is unavailable.
    using NodeWidgetProvider = std::function<QWidget*(QtNodes::NodeId)>;

    /// Creates a QMainWindow whose central widget is `workspace.root` and whose
    /// docks are built from `workspace.docks`. The caller owns the window.
    static QMainWindow* buildWorkspace(const WorkspaceLayout& workspace,
                                       const NodeWidgetProvider& provider);

    /// Builds `workspace` into an existing window: replaces its central widget
    /// (buildNode) and adds its docks. Returns true when at least one widget was
    /// built. Used by RuntimeShell (REQ-SW-PL-053) to apply a v2 layout to the
    /// runtime main window instead of creating a second QMainWindow.
    static bool buildInto(QMainWindow* window,
                          const WorkspaceLayout& workspace,
                          const NodeWidgetProvider& provider);

    /// Reconstructs a WorkspaceLayout from a window produced by buildWorkspace().
    static WorkspaceLayout captureWorkspace(QMainWindow* window);
};

} // namespace FlowUi