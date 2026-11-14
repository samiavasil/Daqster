/************************************************************************
                      Daqster/IMenuProvider.h - Copyright
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

 You should have received a copy of the GNU Library General Public Licence
 along with Daqster; if not, see <http://www.gnu.org/licenses/>.

 Initial version of this file was created on 08.11.2026
 ************************************************************************/
#pragma once

#include <QString>
#include <QVector>

namespace Daqster {

/**
 * @brief One action a plugin contributes to a host menu (REQ-SW-PL-084).
 *
 * A small value type on purpose: the host owns the `QAction`, the plugin only
 * says where the entry belongs, what it is called and which id to send back.
 * `menuPath` is a `/`-separated path of menu titles — `"Tools"` appends to the
 * existing Tools menu, `"Tools/AI Setup"` creates (or reuses) that submenu.
 */
struct PluginMenuAction
{
    QString menuPath;   ///< "Tools", "Tools/Plugins" — "/" = submenu
    QString text;       ///< menu entry text (translated by the provider)
    QString id;         ///< stable id passed back to triggerMenuAction()
    bool    enabled = true; ///< false = shown but greyed out
};

/**
 * @brief The IMenuProvider interface — a plugin's actions in the host's menus
 *        (REQ-SW-PL-084).
 *
 * IStatusBarProvider (REQ-SW-PL-079) already lets a plugin put *status* into
 * the host plus actions in one dedicated "Plugins" menu. That is enough while
 * the only host menu a plugin needs is that one, but not when the entry
 * belongs somewhere else — `Tools → AI Setup` is a Tools entry, not a status
 * bar action, and a plugin cannot add it without the framework knowing the
 * plugin. This capability is that seam: the host renders, the plugin decides.
 *
 * Deliberately **generic**: nothing here is about AI. A plugin that adds
 * "Tools → Rebuild index" or "View → Sensors" uses the same interface.
 *
 * Design constraints (the same ones INodeProvider/IWidgetProvider follow):
 *
 *  - **No QtWidgets.** Plain, non-`QObject` interface returning value types,
 *    so a provider can live in a widget-free core plugin. The host creates
 *    the `QAction`/`QMenu` objects.
 *  - **No ownership transfer.** The host keeps the raw pointer and never
 *    deletes it; the plugin owns the object (normally the plugin object
 *    itself). Discovery uses `dynamic_cast` through
 *    `PluginRegistry::menuProviders()` — no `Q_DECLARE_INTERFACE` IID, exactly
 *    like `IStoppable`.
 *  - **Thread-safety: GUI thread only.** Both methods are called from the
 *    thread that owns the host's widgets.
 *
 * Lifetime: queried when the host builds its menus. A provider that is a
 * `QObject` should call `menuActions()` cheaply — the host may re-read it when
 * plugins are loaded/unloaded.
 */
class IMenuProvider
{
public:
    virtual ~IMenuProvider() = default;

    /**
     * @brief Actions to place in the host's menu bar (may be empty).
     *
     * Called when the host installs plugin menu entries. Must be allocation
     * only: no I/O, no inference, no dialog. Actions with an empty `menuPath`
     * or an empty `id` are skipped by the host with a warning (nothing to
     * attach them to / nothing to send back).
     *
     * @return Actions, in menu order
     */
    virtual QVector<PluginMenuAction> menuActions() const = 0;

    /**
     * @brief Execute one of `menuActions()` by id.
     *
     * Called from the GUI thread when the user picks the entry. An unknown id
     * must be a **no-op** (provider and host can briefly disagree after a
     * reload). A failure must log and return — never throw; the host guards
     * the call, but an exception here has no meaningful recovery.
     *
     * @param id `PluginMenuAction::id` of the triggered entry
     */
    virtual void triggerMenuAction(const QString& id) = 0;
};

} // namespace Daqster
