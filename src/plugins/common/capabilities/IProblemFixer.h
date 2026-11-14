/************************************************************************
                      Daqster/IProblemFixer.h - Copyright
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

 Initial version of this file was created on 09.11.2026
 ************************************************************************/
#pragma once

#include "ISceneOpening.h"

#include <QString>

namespace Daqster {

/**
 * @brief Optional capability: a plugin fixes a problem it reported at scene
 *        open time (REQ-SW-PL-085).
 *
 * `ISceneOpening` (REQ-SW-PL-082) let a plugin *say* why a scene cannot run, and
 * the IDE renders that verdict in `SceneProblemsPanel`. Saying it is half a
 * solution: for "model file is missing" or "this backend is not built" the
 * plugin usually knows exactly what to do and the user has to guess the same
 * thing from a hint. This is the other half: the plugin offers a button with
 * its own label and performs the repair itself, next to the problem it owns.
 *
 * **Why a separate interface and not a method on `ISceneOpening`.** The
 * capability is narrower on purpose: most providers can diagnose but not
 * repair, and adding a virtual to `ISceneOpening` would force every existing
 * implementation to grow a slot. `dynamic_cast` on a new interface is invisible
 * to plugins that do not implement it — they are simply not asked.
 *
 * **The host never fixes anything.** It asks `canFix()`, shows `fixLabel()` on
 * the row and calls `fix()` when the user presses it. Reloading a scene or
 * touching a model is the plugin's business, not the host's.
 *
 * **It may fix more than its own problems.** A fixer is matched by
 * `canFix(problem)`, not by provider identity: several plugins can be asked
 * and the first that says yes wins. Implementations must therefore survive
 * problems they did not produce (return `false` from `canFix()` when unsure).
 *
 * Lifetime and threading: the object belongs to the plugin and is never deleted
 * by the host; discovery is `dynamic_cast` (like `IMenuProvider`), so no
 * `Q_DECLARE_INTERFACE` IID is needed. `canFix()`/`fixLabel()` are called when
 * the panel is filled (GUI thread, must be cheap and allocation only — no I/O,
 * no dialogs); `fix()` is called on the GUI thread from a button press and may
 * fill `error` with the reason when it fails. `fix()` must not throw.
 */
class IProblemFixer
{
public:
    virtual ~IProblemFixer() = default;

    /**
     * @brief Whether this provider can repair @p problem.
     *
     * Must be a cheap, side-effect-free check (string/id comparison, file
     * existence at most). Called once per shown problem.
     *
     * @param problem One `SceneOpenProblem` the scene verdict reported
     * @return true when `fix()` is offered for this problem
     */
    virtual bool canFix(const Daqster::SceneOpenProblem& problem) const = 0;

    /**
     * @brief Text of the button on the problem's row ("Поправи", "Изтегли…").
     *
     * Called only when `canFix(problem)` is true. An **empty** label means
     * "no button" — the host renders the problem as before, so a provider that
     * is not sure yet can stay silent without a second method.
     *
     * @param problem The same problem passed to `canFix()`
     * @return Button text, or an empty string for no button
     */
    virtual QString fixLabel(const Daqster::SceneOpenProblem& problem) const = 0;

    /**
     * @brief Perform the repair.
     *
     * Called from the GUI thread when the user presses the row's button. The
     * host shows a failure in the panel's own status line (never a modal), so
     * @p error must explain what went wrong in the user's words. The host
     * refreshes the problem list on success, so the fixer does not have to
     * touch any widget; it only changes whatever the problem described.
     *
     * @param problem The problem being repaired
     * @param error Receives the failure reason; may be null. Set only on
     *              failure.
     * @return true when the problem is gone (or the repair was started and the
     *         scene can be re-checked), false on failure
     */
    virtual bool fix(const Daqster::SceneOpenProblem& problem, QString* error) = 0;
};

} // namespace Daqster
