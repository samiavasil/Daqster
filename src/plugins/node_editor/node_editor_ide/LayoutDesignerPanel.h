#pragma once

#include "LayoutEngine.h"
#include "LayoutTree.h"

#include <QVector>
#include <QWidget>

#include <functional>

class QComboBox;
class QLineEdit;
class QListWidget;
class QListWidgetItem;
class QMainWindow;
class QToolBar;
class QTreeWidget;
class QTreeWidgetItem;
class QVBoxLayout;

/**
 * Visual Layout Designer (REQ-SW-PL-059).
 *
 * Edits a FlowUi::LayoutSection (schema v2: split/grid/tab/panel + docks) with
 * the mouse and renders it live with FlowUi::LayoutEngine::buildWorkspace().
 *
 * The panel is passive about who owns the graph: the host (NodeEditorIdeObject)
 * supplies
 *   - a Provider that returns the live widget of a node, and
 *   - the list of nodes that have a widget ("Available Widgets").
 *
 * m_section is the single source of truth. Every mutation pushes an undo
 * snapshot, rebuilds the structure tree and rebuilds the live preview.
 *
 * Widget map:
 *   - toolbar                → create split/grid/tab/dock, capture, save, undo
 *   - "Available Widgets"    → drag source (one entry per widget node)
 *   - "Structure" tree       → drop target (workspace/containers/grid cells/tabs/docks)
 *   - "Preview"              → live WYSIWYG built by LayoutEngine
 */
class LayoutDesignerPanel : public QWidget
{
    Q_OBJECT
public:
    explicit LayoutDesignerPanel(QWidget* parent = nullptr);
    ~LayoutDesignerPanel() override;

    using Provider = std::function<QWidget*(QtNodes::NodeId)>;

    /// Live widget provider handed to LayoutEngine::buildWorkspace().
    void setProvider(Provider provider);

    /// Nodes offered in the "Available Widgets" list (id, display label).
    void setAvailableNodes(const QVector<QPair<QtNodes::NodeId, QString>>& nodes);

    /// The section being edited (source of truth).
    FlowUi::LayoutSection layoutSection() const { return m_section; }
    void setLayoutSection(const FlowUi::LayoutSection& section);

    /// Rebuild preview + structure tree (e.g. after the host changed providers).
    void refresh();

    // ── Programmatic commands (also used by the toolbar) ────────────────────
    void addSplit(FlowUi::LayoutNode::Orientation orientation);
    void addGrid();
    void addTab();
    void addDock(FlowUi::DockLayout::Area area);
    void captureCurrent();
    void undo();
    void redo();
    void addWorkspace();
    void removeWorkspace();

    /// Number of drop targets currently offered by the Structure tree.
    int slotCount() const { return m_assigners.size(); }

    /// Assign `nodeId` to the slot at `slotIndex` — the same path the drag&drop
    /// handler uses (InvalidNodeId clears the slot).
    void assignSlot(int slotIndex, QtNodes::NodeId nodeId);

Q_SIGNALS:
    /// The user pressed "Save Layout" — the host serializes layoutSection().
    void saveRequested();
    /// The user pressed "Exit Designer".
    void closed();

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    // ── construction ────────────────────────────────────────────────────────
    void buildUi();
    QToolBar* buildToolbar();

    // ── model helpers ───────────────────────────────────────────────────────
    FlowUi::WorkspaceLayout& currentWorkspace();
    void ensureWorkspace();
    void pushUndo();
    void applyMutation(const std::function<void()>& mutate);

    // ── tree building ───────────────────────────────────────────────────────
    void rebuildStructure();
    void populateNode(QTreeWidgetItem* parent, FlowUi::LayoutNode& node);
    void populateDocks(QTreeWidgetItem* parent, FlowUi::WorkspaceLayout& ws);
    QTreeWidgetItem* addAssignableItem(QTreeWidgetItem* parent,
                                       const QString& label,
                                       std::function<void(QtNodes::NodeId)> assign,
                                       QtNodes::NodeId current);

    // ── preview ─────────────────────────────────────────────────────────────
    void rebuildPreview();
    void clearPreview();

    // ── actions ─────────────────────────────────────────────────────────────
    void removeSelected();

    void rebuildAvailable();
    void refreshWorkspaceCombo();

    // ── state ───────────────────────────────────────────────────────────────
    FlowUi::LayoutSection m_section;
    int m_currentWorkspace = 0;
    bool m_hasSection = false;

    Provider m_provider;
    QVector<QPair<QtNodes::NodeId, QString>> m_available;

    // tree item data → index into m_assigners
    QVector<std::function<void(QtNodes::NodeId)>> m_assigners;

    QVector<FlowUi::LayoutSection> m_undoStack;
    QVector<FlowUi::LayoutSection> m_redoStack;

    QLineEdit* m_search = nullptr;
    QListWidget* m_availableList = nullptr;
    QTreeWidget* m_structure = nullptr;
    QWidget* m_previewHost = nullptr;
    QVBoxLayout* m_previewLayout = nullptr;
    QMainWindow* m_preview = nullptr;
    QComboBox* m_workspaceCombo = nullptr;
};
