#include "NodeEditorWidget.h"
#include "ChatGraphModel.h"

#include <QVBoxLayout>

#include <QtNodes/NodeDelegateModelRegistry>
#include <QtNodes/DataFlowGraphModel>
#include <QtNodes/DataFlowGraphicsScene>
#include <QtNodes/GraphicsView>
#include <QtNodes/BasicGraphicsScene>
#include <QtNodes/ConnectionStyle>

using namespace QtNodes;

NodeEditorWidget::NodeEditorWidget(QWidget* parent)
    : QWidget(parent)
    , m_registry(std::make_shared<NodeDelegateModelRegistry>())
{
    m_layout = new QVBoxLayout(this);
    m_layout->setContentsMargins(0, 0, 0, 0);
    m_layout->setSpacing(0);
}

NodeEditorWidget::~NodeEditorWidget() = default;

NodeDelegateModelRegistry* NodeEditorWidget::getInjectedRegistry() const
{
    return m_registry.get();
}

void NodeEditorWidget::setConnectionStyle(const QString& json)
{
    ConnectionStyle::setConnectionStyle(json);
}

void NodeEditorWidget::buildCanvas()
{
    if (m_canvasBuilt)
        return;

    m_graphModel = new ChatGraphModel(m_registry);
    m_scene = new DataFlowGraphicsScene(*m_graphModel, this);
    m_view = new GraphicsView(m_scene, this);

    m_layout->addWidget(m_view);

    connect(m_scene, &BasicGraphicsScene::nodeDoubleClicked,
            this, &NodeEditorWidget::nodeDoubleClicked);

    m_canvasBuilt = true;
}
