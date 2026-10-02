#include "NodeEditorWidget.h"
#include "ChatGraphModel.h"
#include "CustomDataFlowScene.h"
#include "capabilities/IWidgetProvider.h"
#include "capabilities/IStoppable.h"

#include "LogCategories.h"

#include <QVBoxLayout>

#include <QtNodes/NodeDelegateModelRegistry>
#include <QtNodes/NodeDelegateModel>
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

void NodeEditorWidget::setWidgetProvider(Daqster::IWidgetProvider* provider)
{
    // The graph model is created in buildCanvas(), so we set the provider there.
    // If buildCanvas() was already called, we can set it immediately.
    if (m_graphModel) {
        if (auto* chatModel = dynamic_cast<ChatGraphModel*>(m_graphModel)) {
            chatModel->setWidgetProvider(provider);
        }
    }
    // Store for later if buildCanvas() hasn't been called yet
    m_widgetProvider = provider;
}

QWidget* NodeEditorWidget::nodeWidget(QtNodes::NodeId nodeId) const
{
    if (!m_graphModel)
        return nullptr;

    // nodeData() is the virtual hook ChatGraphModel uses to serve widgets from
    // the IWidgetProvider, so this call works through the base-class pointer.
    return m_graphModel->nodeData(nodeId, QtNodes::NodeRole::Widget).value<QWidget*>();
}

void NodeEditorWidget::stopAllNodes()
{
    if (!m_graphModel)
        return;

    for (const QtNodes::NodeId nodeId : m_graphModel->allNodeIds()) {
        auto* model = m_graphModel->delegateModel<QtNodes::NodeDelegateModel>(nodeId);
        if (!model)
            continue;

        if (auto* stoppable = dynamic_cast<Daqster::IStoppable*>(model)) {
            qCInfo(lcNodeEditor) << "Stopping node:" << model->caption() << "(id=" << nodeId << ")";
            stoppable->stop();
        }
    }
}

void NodeEditorWidget::buildCanvas()
{
    if (m_canvasBuilt)
        return;

    m_graphModel = new ChatGraphModel(m_registry);
    // Pass the widget provider to the graph model (REQ-SW-PL-051)
    if (m_widgetProvider) {
        static_cast<ChatGraphModel*>(m_graphModel)->setWidgetProvider(m_widgetProvider);
    }
    m_scene = new CustomDataFlowScene(*m_graphModel, this);
    m_view = new GraphicsView(m_scene, this);

    m_layout->addWidget(m_view);

    connect(m_scene, &BasicGraphicsScene::nodeDoubleClicked,
            this, &NodeEditorWidget::nodeDoubleClicked);

    m_canvasBuilt = true;
}