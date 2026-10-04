#include "LayoutEngine.h"

#include <QDockWidget>
#include <QGridLayout>
#include <QLayout>
#include <QLayoutItem>
#include <QList>
#include <QSplitter>
#include <QTabWidget>
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

void tagWidget(QWidget* widget, const char* type)
{
    if (widget)
        widget->setProperty(kNodeType, QString::fromLatin1(type));
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

QWidget* buildNode(const LayoutNode& node, const LayoutEngine::NodeWidgetProvider& provider);

QWidget* buildPanel(const LayoutNode& node, const LayoutEngine::NodeWidgetProvider& provider)
{
    if (node.nodeId == QtNodes::InvalidNodeId)
        return nullptr;

    QWidget* widget = provider ? provider(node.nodeId) : nullptr;
    if (!widget)
        return nullptr;

    tagWidget(widget, "panel");
    widget->setProperty(kNodeId, static_cast<uint>(node.nodeId));
    widget->setProperty(kPanelTitle, node.title);
    return widget;
}

QWidget* buildSplit(const LayoutNode& node, const LayoutEngine::NodeWidgetProvider& provider)
{
    const Qt::Orientation orientation = node.orientation == LayoutNode::Orientation::Vertical
        ? Qt::Vertical
        : Qt::Horizontal;
    auto* splitter = new QSplitter(orientation);
    tagWidget(splitter, "split");

    for (const LayoutNode& child : node.children) {
        if (QWidget* childWidget = buildNode(child, provider))
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

QWidget* buildGrid(const LayoutNode& node, const LayoutEngine::NodeWidgetProvider& provider)
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

    const int cellCount = node.rows * node.cols;
    for (int index = 0; index < cellCount; ++index) {
        if (index >= static_cast<int>(node.cells.size()) || !node.cells[index].has_value())
            continue; // empty cell — leave the grid position unused

        QWidget* widget = provider ? provider(*node.cells[index]) : nullptr;
        if (!widget)
            continue; // provider could not supply the node — skip cell

        tagWidget(widget, "panel");
        widget->setProperty(kNodeId, static_cast<uint>(*node.cells[index]));
        layout->addWidget(widget, index / node.cols, index % node.cols);
    }
    return container;
}

QWidget* buildTab(const LayoutNode& node, const LayoutEngine::NodeWidgetProvider& provider)
{
    auto* tabs = new QTabWidget();
    tagWidget(tabs, "tab");

    for (const LayoutNode& child : node.children) {
        QWidget* childWidget = buildNode(child, provider);
        if (!childWidget)
            continue;
        tabs->addTab(childWidget, displayTitle(child, childWidget));
    }

    if (tabs->count() == 0) {
        delete tabs;
        return nullptr;
    }

    if (node.currentIndex >= 0 && node.currentIndex < tabs->count())
        tabs->setCurrentIndex(node.currentIndex);
    return tabs;
}

QWidget* buildNode(const LayoutNode& node, const LayoutEngine::NodeWidgetProvider& provider)
{
    switch (node.type) {
    case LayoutNode::Type::Panel: return buildPanel(node, provider);
    case LayoutNode::Type::Split: return buildSplit(node, provider);
    case LayoutNode::Type::Grid:  return buildGrid(node, provider);
    case LayoutNode::Type::Tab:   return buildTab(node, provider);
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
                if (cellWidget && cellWidget->property(kNodeId).isValid())
                    node.cells.push_back(
                        static_cast<QtNodes::NodeId>(cellWidget->property(kNodeId).toUInt()));
                else
                    node.cells.push_back(std::nullopt);
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
                                          const NodeWidgetProvider& provider)
{
    auto* window = new QMainWindow();
    window->setProperty(kWorkspaceId, workspace.id);
    window->setProperty(kWorkspaceName, workspace.name);

    if (QWidget* central = buildNode(workspace.root, provider))
        window->setCentralWidget(central);

    for (const DockLayout& dockLayout : workspace.docks) {
        QWidget* widget = provider ? provider(dockLayout.node) : nullptr;
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
    }
    return window;
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