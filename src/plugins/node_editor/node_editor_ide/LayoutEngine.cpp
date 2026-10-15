#include "LayoutEngine.h"

#include <QDockWidget>
#include <QFrame>
#include <QGridLayout>
#include <QLabel>
#include <QLayout>
#include <QLayoutItem>
#include <QList>
#include <QSplitter>
#include <QTabWidget>
#include <QVBoxLayout>
#include <QWidget>

#include <QString>

namespace FlowUi {

namespace {

// Property tags written during build and read back during capture.
const char* const kNodeType = "layoutNodeType";
const char* const kNodeId = "layoutNodeId";
const char* const kPanelTitle = "layoutPanelTitle";
const char* const kGridRows = "layoutGridRows";
const char* const kGridCols = "layoutGridCols";
const char* const kDockNode = "layoutDockNode";
const char* const kDockTitle = "layoutDockTitle";
const char* const kDockArea = "layoutDockArea";
const char* const kWorkspaceId = "layoutWorkspaceId";
const char* const kWorkspaceName = "layoutWorkspaceName";
// Designer-only: marks an empty-slot placeholder. It is deliberately NOT tagged
// with kNodeType, so captureNode() reads it back as an empty Panel (InvalidNodeId).
const char* const kPlaceholder = "layoutPlaceholder";

void tagWidget(QWidget* widget, const char* type)
{
    if (widget)
        widget->setProperty(kNodeType, QString::fromLatin1(type));
}

/// A visible stand-in for an empty panel/grid cell, shown only when
/// BuildOptions::placeholders is set (Layout Designer preview). Runtime never
/// builds these, so empty slots keep taking no space there.
QWidget* makePlaceholder(const QString& text)
{
    auto* frame = new QFrame();
    frame->setProperty(kPlaceholder, true);
    frame->setFrameShape(QFrame::StyledPanel);
    frame->setMinimumSize(56, 36);
    frame->setStyleSheet(QStringLiteral(
        "QFrame[layoutPlaceholder=\"true\"] {"
        " border: 1px dashed #9aa0a6; background: #f1f3f4; }"));
    auto* layout = new QVBoxLayout(frame);
    layout->setContentsMargins(2, 2, 2, 2);
    auto* label = new QLabel(text, frame);
    label->setAlignment(Qt::AlignCenter);
    label->setWordWrap(true);
    label->setStyleSheet(QStringLiteral("color: #5f6368; border: none;"));
    layout->addWidget(label);
    return frame;
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
    return DockLayout::Area::Right;
}

Qt::DockWidgetArea toDockArea(DockLayout::Area area)
{
    switch (area) {
    case DockLayout::Area::Left:   return Qt::LeftDockWidgetArea;
    case DockLayout::Area::Top:    return Qt::TopDockWidgetArea;
    case DockLayout::Area::Bottom: return Qt::BottomDockWidgetArea;
    case DockLayout::Area::Right:  // fall through
    default:                       return Qt::RightDockWidgetArea;
    }
}

/// Human-readable label for a container's tab/dock title. Prefers the explicit
/// layout title, then the widget's window title, then "Node N" for panels.
QString displayTitle(const LayoutNode& node, QWidget* widget)
{
    if (!node.title.isEmpty())
        return node.title;
    if (widget && !widget->windowTitle().isEmpty())
        return widget->windowTitle();
    if (node.type == LayoutNode::Type::Panel && node.nodeId != QtNodes::InvalidNodeId)
        return QObject::tr("Node %1").arg(node.nodeId);
    return QString();
}

QWidget* buildNode(const LayoutNode& node,
                   const LayoutEngine::NodeWidgetProvider& provider,
                   const LayoutEngine::BuildOptions& options);

QWidget* buildPanel(const LayoutNode& node,
                    const LayoutEngine::NodeWidgetProvider& provider,
                    const LayoutEngine::BuildOptions& options)
{
    if (node.nodeId == QtNodes::InvalidNodeId)
        return options.placeholders ? makePlaceholder(QObject::tr("empty")) : nullptr;

    QWidget* widget = provider ? provider(node.nodeId) : nullptr;
    if (!widget)
        return options.placeholders
                   ? makePlaceholder(QObject::tr("node #%1 (no widget)").arg(node.nodeId))
                   : nullptr;

    tagWidget(widget, "panel");
    widget->setProperty(kNodeId, static_cast<uint>(node.nodeId));
    widget->setProperty(kPanelTitle, node.title);
    return widget;
}

QWidget* buildSplit(const LayoutNode& node,
                    const LayoutEngine::NodeWidgetProvider& provider,
                    const LayoutEngine::BuildOptions& options)
{
    const Qt::Orientation orientation = node.orientation == LayoutNode::Orientation::Vertical
        ? Qt::Vertical
        : Qt::Horizontal;
    auto* splitter = new QSplitter(orientation);
    tagWidget(splitter, "split");

    for (const LayoutNode& child : node.children) {
        if (QWidget* childWidget = buildNode(child, provider, options))
            splitter->addWidget(childWidget);
    }

    if (splitter->count() == 0) {
        delete splitter;
        return nullptr;
    }

    if (!node.sizes.empty()) {
        QList<int> sizes;
        sizes.reserve(static_cast<int>(node.sizes.size()));
        for (int size : node.sizes)
            sizes.append(size);
        splitter->setSizes(sizes);
    }
    return splitter;
}

QWidget* buildGrid(const LayoutNode& node,
                   const LayoutEngine::NodeWidgetProvider& provider,
                   const LayoutEngine::BuildOptions& options)
{
    if (node.rows <= 0 || node.cols <= 0)
        return nullptr;

    auto* container = new QWidget();
    tagWidget(container, "grid");
    container->setProperty(kGridRows, node.rows);
    container->setProperty(kGridCols, node.cols);

    auto* layout = new QGridLayout(container);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    // Ensure equal space distribution for all cells — critical for QOpenGLWidget
    // children (VideoDisplayWidget) which have 0 size hints and need explicit
    // stretch to get paint events and visible space.
    for (int r = 0; r < node.rows; ++r)
        layout->setRowStretch(r, 1);
    for (int c = 0; c < node.cols; ++c)
        layout->setColumnStretch(c, 1);

    const int cellCount = node.rows * node.cols;
    for (int index = 0; index < cellCount; ++index) {
        const int row = index / node.cols;
        const int col = index % node.cols;

        const LayoutNode* child = index < static_cast<int>(node.children.size())
            ? &node.children[index]
            : nullptr;

        // buildNode() already renders an "empty" placeholder for an unassigned
        // Panel and a "node #N (no widget)" placeholder for a Panel whose
        // provider returned nothing, so a single call covers all cases.
        QWidget* widget = child ? buildNode(*child, provider, options) : nullptr;
        if (!widget && options.placeholders)
            widget = makePlaceholder(QObject::tr("empty"));
        if (widget)
            layout->addWidget(widget, row, col);
    }
    return container;
}

QWidget* buildTab(const LayoutNode& node,
                  const LayoutEngine::NodeWidgetProvider& provider,
                  const LayoutEngine::BuildOptions& options)
{
    auto* tabs = new QTabWidget();
    tagWidget(tabs, "tab");

    int page = 1;
    for (const LayoutNode& child : node.children) {
        QWidget* childWidget = buildNode(child, provider, options);
        if (!childWidget)
            continue;
        QString title = displayTitle(child, childWidget);
        if (title.isEmpty())
            title = QObject::tr("Page %1").arg(page);
        tabs->addTab(childWidget, title);
        ++page;
    }

    if (tabs->count() == 0) {
        delete tabs;
        return nullptr;
    }

    if (node.currentIndex >= 0 && node.currentIndex < tabs->count())
        tabs->setCurrentIndex(node.currentIndex);
    return tabs;
}

QWidget* buildNode(const LayoutNode& node,
                   const LayoutEngine::NodeWidgetProvider& provider,
                   const LayoutEngine::BuildOptions& options)
{
    switch (node.type) {
    case LayoutNode::Type::Panel: return buildPanel(node, provider, options);
    case LayoutNode::Type::Split: return buildSplit(node, provider, options);
    case LayoutNode::Type::Grid:  return buildGrid(node, provider, options);
    case LayoutNode::Type::Tab:   return buildTab(node, provider, options);
    }
    return nullptr;
}

LayoutNode captureNode(QWidget* widget)
{
    LayoutNode node;
    if (!widget)
        return node;

    const QString type = widget->property(kNodeType).toString();

    if (type == QLatin1String("split")) {
        node.type = LayoutNode::Type::Split;
        auto* splitter = qobject_cast<QSplitter*>(widget);
        if (splitter) {
            node.orientation = splitter->orientation() == Qt::Vertical
                ? LayoutNode::Orientation::Vertical
                : LayoutNode::Orientation::Horizontal;
            const QList<int> sizes = splitter->sizes();
            node.sizes.assign(sizes.constBegin(), sizes.constEnd());
            for (int i = 0; i < splitter->count(); ++i)
                node.children.push_back(captureNode(splitter->widget(i)));
        }
    } else if (type == QLatin1String("grid")) {
        node.type = LayoutNode::Type::Grid;
        node.rows = widget->property(kGridRows).toInt();
        node.cols = widget->property(kGridCols).toInt();
        auto* layout = qobject_cast<QGridLayout*>(widget->layout());
        if (layout && node.rows > 0 && node.cols > 0) {
            const int cellCount = node.rows * node.cols;
            for (int index = 0; index < cellCount; ++index) {
                QLayoutItem* item = layout->itemAtPosition(index / node.cols, index % node.cols);
                QWidget* cellWidget = item ? item->widget() : nullptr;
                // Capture each cell as a LayoutNode. A cell holding a nested
                // container is captured recursively; an empty cell or a
                // designer placeholder (untagged) comes back as an empty Panel.
                node.children.push_back(captureNode(cellWidget));
            }
        }
    } else if (type == QLatin1String("tab")) {
        node.type = LayoutNode::Type::Tab;
        auto* tabs = qobject_cast<QTabWidget*>(widget);
        if (tabs) {
            node.currentIndex = tabs->currentIndex();
            for (int i = 0; i < tabs->count(); ++i)
                node.children.push_back(captureNode(tabs->widget(i)));
        }
    } else {
        node.type = LayoutNode::Type::Panel;
        if (widget->property(kNodeId).isValid())
            node.nodeId = static_cast<QtNodes::NodeId>(widget->property(kNodeId).toUInt());
        node.title = widget->property(kPanelTitle).toString();
    }
    return node;
}

} // namespace

QMainWindow* LayoutEngine::buildWorkspace(const WorkspaceLayout& workspace,
                                          const NodeWidgetProvider& provider,
                                          const BuildOptions& options)
{
    auto* window = new QMainWindow();
    buildInto(window, workspace, provider, options);
    return window;
}

bool LayoutEngine::buildInto(QMainWindow* window,
                             const WorkspaceLayout& workspace,
                             const NodeWidgetProvider& provider,
                             const BuildOptions& options)
{
    if (!window)
        return false;

    window->setProperty(kWorkspaceId, workspace.id);
    window->setProperty(kWorkspaceName, workspace.name);

    // Track the widgets handed over by the provider so their leftover window
    // state can be cleaned up once the engine has reparented them (below).
    QList<QWidget*> adopted;
    auto trackProvider = [&provider, &adopted](QtNodes::NodeId nodeId) -> QWidget* {
        QWidget* widget = provider ? provider(nodeId) : nullptr;
        if (widget != nullptr && !adopted.contains(widget))
            adopted.append(widget);
        return widget;
    };

    bool built = false;

    if (QWidget* central = buildNode(workspace.root, trackProvider, options)) {
        window->setCentralWidget(central);
        built = true;
    }

    for (const DockLayout& dockLayout : workspace.docks) {
        QWidget* widget = trackProvider(dockLayout.node);
        if (!widget)
            continue;

        QString title = dockLayout.title;
        if (title.isEmpty())
            title = widget->windowTitle();
        if (title.isEmpty())
            title = QObject::tr("Node %1").arg(dockLayout.node);

        auto* dock = new QDockWidget(title, window);
        dock->setWidget(widget);
        tagWidget(dock, "dock");
        dock->setProperty(kDockNode, static_cast<uint>(dockLayout.node));
        dock->setProperty(kDockTitle, dockLayout.title);
        dock->setProperty(kDockArea, areaToString(dockLayout.area));

        window->addDockWidget(toDockArea(dockLayout.area), dock);

        if (dockLayout.floating)
            dock->setFloating(true);
        if (dockLayout.geometry.w > 0 && dockLayout.geometry.h > 0) {
            dock->setGeometry(QRect(dockLayout.geometry.x, dockLayout.geometry.y,
                                    dockLayout.geometry.w, dockLayout.geometry.h));
        }
        built = true;
    }

    // Node widgets are handed over deembedded: their graph floats them as
    // top-level windows (NodeGraphicsObject::setWidgetEmbedded(false)), which
    // gives each its own QWindow. The engine has just reparented them into
    // `window`, so the leftover window flags can be dropped now. Clearing them
    // while the widget was still parentless made Qt skip the window-to-child
    // cleanup, leaving a stale QWindow behind: mapToGlobal() then kept using
    // that stale window, so popups opened far from the mouse and clicks /
    // hit-tests missed their target.
    for (QWidget* widget : adopted) {
        if (widget->windowFlags() & (Qt::Window | Qt::WindowStaysOnTopHint))
            widget->setWindowFlags(Qt::Widget);
    }

    return built;
}

WorkspaceLayout LayoutEngine::captureWorkspace(QMainWindow* window)
{
    WorkspaceLayout workspace;
    if (!window)
        return workspace;

    workspace.id = window->property(kWorkspaceId).toInt();
    workspace.name = window->property(kWorkspaceName).toString();

    if (QWidget* central = window->centralWidget())
        workspace.root = captureNode(central);

    const QList<QDockWidget*> docks = window->findChildren<QDockWidget*>();
    for (QDockWidget* dock : docks) {
        DockLayout dockLayout;
        dockLayout.node = static_cast<QtNodes::NodeId>(dock->property(kDockNode).toUInt());
        dockLayout.title = dock->property(kDockTitle).toString();
        dockLayout.area = areaFromString(dock->property(kDockArea).toString());
        dockLayout.floating = dock->isFloating();

        const QRect geometry = dock->geometry();
        dockLayout.geometry.x = geometry.x();
        dockLayout.geometry.y = geometry.y();
        dockLayout.geometry.w = geometry.width();
        dockLayout.geometry.h = geometry.height();
        dockLayout.geometry.maximized = false;

        workspace.docks.push_back(dockLayout);
    }
    return workspace;
}

} // namespace FlowUi