#pragma once

#include <QWidget>
#include <memory>
#include <QtNodes/Definitions>

#include "capabilities/IWidgetProvider.h"

namespace QtNodes {
class NodeDelegateModelRegistry;
class DataFlowGraphModel;
class DataFlowGraphicsScene;
class GraphicsView;
}

class QVBoxLayout;

class NodeEditorWidget : public QWidget
{
    Q_OBJECT
public:
    explicit NodeEditorWidget(QWidget* parent = nullptr);
    ~NodeEditorWidget();

    QtNodes::NodeDelegateModelRegistry* getInjectedRegistry() const;

    /// Accessor used by the DAQSTER_AUTOSTART_VIDEO dev driver to build a video
    /// source -> VideoOutput graph programmatically.
    QtNodes::DataFlowGraphModel* graphModel() const { return m_graphModel; }

    /// Accessor used by the IDE File menu (REQ-SW-PL-037) to save/load the
    /// scene. The concrete scene is a CustomDataFlowScene, exposed through the
    /// base DataFlowGraphicsScene interface.
    QtNodes::DataFlowGraphicsScene* scene() const { return m_scene; }

    void setConnectionStyle(const QString& json);

    void buildCanvas();

    /// Set the widget provider for creating node widgets (REQ-SW-PL-051).
    /// The provider is owned by DemoNodeEditorNodesGuiObject.
    void setWidgetProvider(Daqster::IWidgetProvider* provider);

    /// The GUI widget of a node, or nullptr when the node has none.
    ///
    /// Goes through the graph model so the IWidgetProvider of the GUI plugin is
    /// consulted (REQ-SW-PL-051) - NodeDelegateModel::embeddedWidget() returns
    /// nullptr for every core/split model. Returns the same instance on every
    /// call; ownership is NOT taken.
    QWidget* nodeWidget(QtNodes::NodeId nodeId) const;

    /// Stop every running node of the graph (IStoppable::stop()).
    ///
    /// The runner does this from its close/aboutToQuit path, so the editor must
    /// do it too: closing the editor window tears the graph down, and without an
    /// explicit stop the node threads/cameras would be left running against a
    /// half-destroyed model. IStoppable::stop() is idempotent, so it is safe to
    /// call this from both the window close and the plugin shutdown path.
    void stopAllNodes();

Q_SIGNALS:
    void nodeDoubleClicked(QtNodes::NodeId nodeId);

private:
    std::shared_ptr<QtNodes::NodeDelegateModelRegistry> m_registry;
    QtNodes::DataFlowGraphModel* m_graphModel = nullptr;
    QtNodes::DataFlowGraphicsScene* m_scene = nullptr;
    QtNodes::GraphicsView* m_view = nullptr;
    QVBoxLayout* m_layout;
    bool m_canvasBuilt = false;
    Daqster::IWidgetProvider* m_widgetProvider = nullptr;
};
