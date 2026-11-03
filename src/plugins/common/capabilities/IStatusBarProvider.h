/************************************************************************
                      Daqster/IStatusBarProvider.h - Copyright
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

 Initial version of this file was created on 03.11.2026
 ************************************************************************/
#pragma once

#include <QList>
#include <QString>

namespace Daqster {

/**
 * @brief The IStatusBarProvider interface — a plugin's global state in the
 *        host's status bar (REQ-SW-PL-079).
 *
 * A plugin can own state that no node represents: cached model weights, a warm
 * inference server, the VRAM those hold, a background index being built. None
 * of it belongs to a single node, so none of it can be shown by a node widget,
 * and the user's only window into it was `nvidia-smi`. This capability gives
 * that state one line in the host's status bar, plus optional actions in a
 * menu — the host renders, the plugin decides what is worth saying.
 *
 * Deliberately **generic**: nothing in this interface is about AI, models or
 * VRAM. A plugin that reports "3 sensors streaming" or "license expires in 9
 * days" is as valid as one reporting "8.1 GB of weights warm".
 *
 * Design constraints (the same ones INodeProvider/IWidgetProvider follow):
 *
 *  - **No QtWidgets.** This is a plain (non-`QObject`) interface that returns
 *    strings, so a provider can be implemented in a widget-free core plugin and
 *    compiled into a headless build. The host creates the widgets.
 *  - **No ownership transfer.** The host keeps the raw pointer and never
 *    deletes it; the plugin owns the object (it is normally the plugin object
 *    itself). Discovery uses `dynamic_cast` through
 *    `PluginRegistry::statusBarProviders()` — the interface declares no
 *    `Q_DECLARE_INTERFACE` IID, exactly like `IStoppable`.
 *  - **Thread-safety: GUI thread only.** All methods are called from the
 *    thread that owns the host's widgets — the same thread that calls
 *    `QPluginManager::statusBarProviders()` in the first place. A provider must
 *    not block on another thread's work to answer; see the cost note on
 *    `statusText()`.
 *
 * Lifetime: the provider is queried until the host window is gone or the plugin
 * object is destroyed. The host notices destruction (`QObject::destroyed` on the
 * plugin object) and drops the row instead of calling into freed memory.
 */
class IStatusBarProvider
{
public:
    virtual ~IStatusBarProvider() = default;

    /**
     * @brief Stable identifier of this row, e.g. `"ai-studio.models"`.
     *
     * Used as the key that ties a provider to its label across refreshes, a
     * prefix in log lines and the disambiguator when two providers fight for
     * the same menu text. Must be unique among the loaded plugins and must not
     * change during the session; empty means "this provider has no stable
     * identity" and the host skips it.
     *
     * @return Unique, session-stable id
     */
    virtual QString statusId() const = 0;

    /**
     * @brief One line for the status bar, e.g. `"Models: 2 warm · 8.1 GB"`.
     *
     * **Called every 2 seconds** for as long as the host window is open, plus
     * once after every scene load/close. It must therefore be cheap: report a
     * value that is already known (a counter, a cached size) and never run
     * inference, enumerate processes or touch the file system to answer. An
     * empty string hides the row — that is the correct answer for "nothing to
     * report right now", and the host treats it as a normal state, not an
     * error.
     *
     * @return Short single-line text; empty hides the row
     */
    virtual QString statusText() const = 0;

    /**
     * @brief Details shown on hover, e.g. the per-model breakdown.
     *
     * May be multi-line (`\n` separated) and may be much longer than
     * `statusText()`; the host shows it as the label's tooltip, so it is only
     * produced when the user actually looks. Same cost rule as `statusText()`
     * unless the host documents otherwise — it is currently fetched together
     * with the text on each refresh.
     *
     * @return Tooltip text; empty means "no extra detail"
     */
    virtual QString statusTooltip() const = 0;

    /**
     * @brief Sort key for the status bar and the menu; lower comes first.
     *
     * Providers that do not care keep the default (100). Ties are broken by
     * `statusId()` so the order is stable across refreshes.
     *
     * @return Order weight, lower is further left
     */
    virtual int statusOrder() const { return 100; }

    /**
     * @brief One menu entry the user can trigger.
     *
     * The action is a small value type on purpose: the host owns the `QAction`,
     * the provider only supplies text and an id. `text` and `tooltip` should be
     * translated by the provider (`tr()`), because it is the one that knows
     * what the action means.
     */
    struct Action
    {
        QString id;      ///< stable id passed back to triggerStatusAction()
        QString text;    ///< menu entry text (translated)
        QString tooltip; ///< menu entry tooltip / status tip (may be empty)
    };

    /**
     * @brief Actions to list in the host's plugin menu (may be empty).
     *
     * Read when the host builds its menu and re-read on every refresh, but only
     * as a cheap fingerprint of the **ids** — the menu itself is rebuilt solely
     * when that fingerprint changes, so an open menu is never torn down under
     * the user's mouse. Producing the list must therefore stay allocation-only
     * (no I/O, no inference).
     *
     * @return Actions, in menu order
     */
    virtual QList<Action> statusActions() const { return {}; }

    /**
     * @brief Execute one of `statusActions()` by id.
     *
     * Called from the GUI thread when the user picks the entry. An unknown id
     * must be a **no-op** (the provider and the host can briefly disagree after
     * a reload); a failing action must log and return, never throw — the host
     * guards the call, but an exception here has no meaningful recovery.
     *
     * @param id `Action::id` of the triggered entry
     */
    virtual void triggerStatusAction(const QString& id) = 0;
};

} // namespace Daqster
