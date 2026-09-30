/************************************************************************
                          Daqster/IWidgetProvider.h - Copyright
 Daqster software
 Copyright (C) 2016, Vasil Vasilev,  Bulgaria

 This file is part of Daqster and its software development toolkit.

 Daqster is a free software; you can redistribute it and/or modify it
 under the terms of the GNU Library General Public Licence as published by
 the Free Software Foundation; either version 2 of the Licence, or (at
 your option) any later version.

 Daqster is distributed in the hope that it will be useful, but
 WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Library
 General Public Licence for more details.

 Initial version of this file was created on 29.09.2026
 ************************************************************************/
#ifndef IWIDGETPROVIDER_H
#define IWIDGETPROVIDER_H

namespace QtNodes {
class NodeDelegateModel;
}

class QWidget;

namespace Daqster {

/**
 * @brief The IWidgetProvider interface
 *
 * Capability interface for plugins that supply the GUI widget of a node
 * whose model lives in a widget-free core plugin (REQ-SW-PL-051 core/gui
 * split).
 *
 * The node editor asks every discovered IWidgetProvider for the widget of a
 * node BEFORE falling back to NodeDelegateModel::embeddedWidget(). This is
 * what lets a core model return nullptr from embeddedWidget() while the
 * application still shows controls: the core stays QtWidgets-free and the
 * GUI plugin owns every QWidget.
 *
 * A provider returns nullptr when it has no widget for that model — the
 * editor then treats the node as having no embedded widget, exactly as
 * before the split.
 *
 * NOTE: like INodeProvider this is deliberately a plain (non-QObject)
 * interface, so discovery uses dynamic_cast via PluginRegistry rather than
 * QPluginManager::instances(IWidgetProvider_IID).
 */
class IWidgetProvider {
public:
    virtual ~IWidgetProvider() = default;

    /**
     * @brief Create (or return) the GUI widget for the given node model.
     *
     * @param model The node model. Never null.
     * @return The widget owned by this provider, or nullptr if this provider
     *         has no widget for that model.
     */
    virtual QWidget* createWidget(QtNodes::NodeDelegateModel* model) const = 0;
};

} // namespace Daqster

#define IWidgetProvider_IID "org.daqster.IWidgetProvider/1.0"
Q_DECLARE_INTERFACE(Daqster::IWidgetProvider, IWidgetProvider_IID)

#endif // IWIDGETPROVIDER_H
