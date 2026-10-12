#pragma once

#include <QtNodes/DataFlowGraphModel>

#include <QHash>
#include <QList>
#include <QPointer>

#include "capabilities/IWidgetProvider.h"

class QWidget;

class ChatGraphModel : public QtNodes::DataFlowGraphModel
{
public:
    using QtNodes::DataFlowGraphModel::DataFlowGraphModel;

    ChatGraphModel(std::shared_ptr<QtNodes::NodeDelegateModelRegistry> registry);

    bool loopsEnabled() const override { return true; }

    /// Set the widget providers for creating node widgets (REQ-SW-PL-051).
    ///
    /// More than one plugin may implement IWidgetProvider (each GUI plugin
    /// serves widgets for ITS OWN core models): the node editor must consult
    /// ALL of them, in order, and take the first widget a provider returns.
    /// Providers are owned by their plugin objects (NodeditorNodesGuiObject,
    /// AiStudioPluginGuiObject, ...) and must outlive this model.
    void setWidgetProviders(const QList<Daqster::IWidgetProvider*>& providers);

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
    /// Widget providers from every GUI plugin that implements IWidgetProvider.
    /// Consulted in order; the first provider that returns a widget wins.
    QList<Daqster::IWidgetProvider*> m_widgetProviders;

    /// One widget instance per node, created on first request.
    mutable QHash<QtNodes::NodeId, QPointer<QWidget>> m_widgetCache;
};
