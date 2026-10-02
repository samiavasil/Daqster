#include "ChatGraphModel.h"

#include <QtNodes/NodeDelegateModel>

#include <QWidget>

ChatGraphModel::ChatGraphModel(std::shared_ptr<QtNodes::NodeDelegateModelRegistry> registry)
    : QtNodes::DataFlowGraphModel(registry)
{
    // Drop the cached widget together with the node. The widget itself is owned
    // by the QGraphicsProxyWidget that embeds it, so we only forget the pointer.
    connect(this, &QtNodes::AbstractGraphModel::nodeDeleted, this, [this](QtNodes::NodeId nodeId) {
        m_widgetCache.remove(nodeId);
    });
}

void ChatGraphModel::setWidgetProvider(Daqster::IWidgetProvider* provider)
{
    if (m_widgetProvider == provider)
        return;

    m_widgetProvider = provider;

    // The widgets already handed out belong to the previous provider, so they
    // must not be served again. We cannot delete them (a proxy may own them),
    // so we only drop the references; already embedded widgets keep working.
    m_widgetCache.clear();
}

QWidget* ChatGraphModel::nodeWidget(QtNodes::NodeId nodeId) const
{
    auto it = m_widgetCache.constFind(nodeId);
    if (it != m_widgetCache.constEnd()) {
        if (QWidget* cached = it.value())
            return cached;
        m_widgetCache.erase(it);
    }

    if (!nodeExists(nodeId))
        return nullptr;

    // delegateModel() is not const in the base class, but we only read from the
    // model, not modify it.
    QtNodes::NodeDelegateModel* model = nullptr;
    try {
        model = const_cast<ChatGraphModel*>(this)->delegateModel<QtNodes::NodeDelegateModel>(nodeId);
    } catch (...) {
        model = nullptr;
    }

    if (model == nullptr)
        return nullptr;

    QWidget* widget = nullptr;

    // First try the widget provider (for split models, REQ-SW-PL-051)
    if (m_widgetProvider) {
        try {
            widget = m_widgetProvider->createWidget(model);
        } catch (...) {
            widget = nullptr;
        }
    }

    // Fallback: models that still build their own widget (e.g. VideoOutput)
    if (widget == nullptr) {
        try {
            widget = model->embeddedWidget();
        } catch (...) {
            widget = nullptr;
        }
    }

    if (widget)
        m_widgetCache.insert(nodeId, widget);

    return widget;
}

QVariant ChatGraphModel::nodeData(QtNodes::NodeId nodeId, QtNodes::NodeRole role) const
{
    if (role == QtNodes::NodeRole::Widget) {
        QWidget* widget = nodeWidget(nodeId);
        if (widget)
            return QVariant::fromValue(widget);
        return QVariant(); // No widget
    }
    return QtNodes::DataFlowGraphModel::nodeData(nodeId, role);
}
