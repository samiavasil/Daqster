#pragma once

#include "NodeEditorWidget_global.h"
#include <QWidget>
#include <memory>
#include <QtNodes/Definitions>

namespace QtNodes {
class NodeDelegateModelRegistry;
class DataFlowGraphModel;
class DataFlowGraphicsScene;
class GraphicsView;
}

class QVBoxLayout;

class NODE_EDITOR_WIDGET_EXPORT NodeEditorWidget : public QWidget
{
    Q_OBJECT
public:
    explicit NodeEditorWidget(QWidget* parent = nullptr);
    ~NodeEditorWidget();

    QtNodes::NodeDelegateModelRegistry* getInjectedRegistry() const;

    void setConnectionStyle(const QString& json);

    void buildCanvas();

Q_SIGNALS:
    void nodeDoubleClicked(QtNodes::NodeId nodeId);

private:
    std::shared_ptr<QtNodes::NodeDelegateModelRegistry> m_registry;
    QtNodes::DataFlowGraphModel* m_graphModel = nullptr;
    QtNodes::DataFlowGraphicsScene* m_scene = nullptr;
    QtNodes::GraphicsView* m_view = nullptr;
    QVBoxLayout* m_layout;
    bool m_canvasBuilt = false;
};
