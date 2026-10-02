#pragma once

#include <QtNodes/DataFlowGraphModel>

#include <QHash>
#include <QPointer>

#include "capabilities/IWidgetProvider.h"

class QWidget;

class ChatGraphModel : public QtNodes::DataFlowGraphModel
{
public:
    using QtNodes::DataFlowGraphModel::DataFlowGraphModel;

    ChatGraphModel(std::shared_ptr<QtNodes::NodeDelegateModelRegistry> registry);

    bool loopsEnabled() const override { return true; }

    /// Set the widget provider for creating node widgets (REQ-SW-PL-051).
    /// The provider is owned by DemoNodeEditorNodesGuiObject.
    void setWidgetProvider(Daqster::IWidgetProvider* provider);

    /// Provider-aware accessor for the GUI widget of a node (REQ-SW-PL-051).
    ///
    /// Returns the SAME instance on every call for a given node: the widget is
    /// created once (from the IWidgetProvider, or from the model itself when no
    /// provider serves it) and then cached until the node is deleted. QtNodes
    /// asks for NodeRole::Widget several times per node creation (embed, size
    /// recomputation, resize, deembed), and every caller expects the node's
    /// single widget - handing out a fresh one each time left the node box
    /// sized after a throwaway widget and leaked one widget per query.
    ///
    /// Ownership stays with whoever took the widget (QGraphicsProxyWidget when
    /// embedded, the caller when detached). The QPointer entry simply goes
    /// null if someone else destroys it and the widget is rebuilt on demand.
    QWidget* nodeWidget(QtNodes::NodeId nodeId) const;

    // Override nodeData() to provide widgets from the provider for models that
    // return nullptr from embeddedWidget() (REQ-SW-PL-051 core/gui split).
    QVariant nodeData(QtNodes::NodeId nodeId, QtNodes::NodeRole role) const override;

private:
    Daqster::IWidgetProvider* m_widgetProvider = nullptr;

    /// One widget instance per node, created on first request.
    mutable QHash<QtNodes::NodeId, QPointer<QWidget>> m_widgetCache;
};
