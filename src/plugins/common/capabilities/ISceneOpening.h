/************************************************************************
                          Daqster/ISceneOpening.h - Copyright
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

 Initial version of this file was created on 04.11.2026
 ************************************************************************/
#pragma once

#include <QJsonObject>
#include <QString>
#include <QVector>

namespace Daqster {

/**
 * @brief One reason a scene cannot be opened, addressed to something the user
 *        can see.
 *
 * A flat string cannot say *which* node is at fault, so a host could only print
 * a list and let the user search the graph. The node id makes the problem
 * actionable: the host can select, highlight or mark the node (REQ-SW-PL-082).
 */
struct SceneOpenProblem
{
    /// Id of the node in the scene (`nodes[].id`). May be empty when the problem
    /// concerns the scene as a whole (a missing settings entry, say).
    QString nodeId;
    /// Model or title, for the human reading the message ("Chat", "Tts").
    QString nodeName;
    /// What is wrong, in the plugin's own words.
    QString message;
    /// What the person should do about it. May be empty.
    QString hint;
};

/**
 * @brief What a plugin answered about a scene that is about to be opened.
 *
 * A scene can be refused for reasons that only the plugin knows: a model file
 * the scene names is missing, the requested backend is not built on this
 * machine, a node type's licence expired. The host cannot discover any of that
 * from the JSON, and discovering it *after* `load()` means the running scene is
 * already gone (REQ-SW-PL-082).
 *
 * The verdict carries **all** problems, not the first: a user fixing one
 * missing path at a time is a bad way to spend an afternoon.
 */
struct SceneOpenVerdict
{
    /// Empty == accepted. Every reason the scene cannot be opened, in the
    /// plugin's own words; the host renders and logs them verbatim.
    QVector<SceneOpenProblem> problems;

    /// True when there is nothing to report (an empty answer is acceptance).
    bool accepted() const { return problems.isEmpty(); }
};

/**
 * @brief Optional plugin-level hook for the **open** half of a scene lifecycle.
 *
 * The hosts ask this **before the graph is loaded** — before `load()`,
 * `clearScene()` and before the outgoing scene is stopped, so the nodes of the
 * new scene do not exist yet and the old ones are still exactly as they were.
 * A refusal therefore costs nothing: nothing was stopped, nothing was
 * destroyed, the running scene keeps running.
 *
 * **Why this is a separate interface and not a slot on `ISceneLifecycle`.**
 * `ISceneLifecycle` declares no `Q_DECLARE_INTERFACE` IID and no version
 * (`capabilities/ISceneLifecycle.h`), so it has no compatibility contract: all
 * a host knows is the vtable layout the plugin was compiled against. Adding a
 * virtual there means a plugin built against the old header is called through a
 * slot that does not exist in its vtable — an indirect call into whatever
 * happens to sit after the last slot (the INC-014 class of defect, and the same
 * reason `INodeProvider.h` must stay byte-identical). A new interface probed
 * with `dynamic_cast` is invisible to plugins that do not implement it: they
 * simply are not asked.
 *
 * **What it must not do.** This hook runs while the user is looking at a
 * "loading…" moment and before anything exists: it must not start processes,
 * threads or load models, and it must not block on work owned by another
 * thread. Answer from what is already known (settings, file existence, build
 * configuration) — the real preparation happens later, in the nodes' `start()`.
 *
 * **It must answer without `Initialize()` having run.** The host reaches the
 * providers through `QPluginManager::nodeProviders()` /
 * `PluginRegistry::nodeProviders()`, which deliberately does **not** call
 * `ensureInitialized()` (`PluginRegistry.cpp:232-239`): initialising a plugin
 * here would build runtime hosts (a whole `RuntimeShell` with MDI, widgets and
 * OpenGL) just to ask a question. Settings must therefore be read lazily, from
 * `SettingsStore` / the environment, never from state that only `Initialize()`
 * populates.
 *
 * Discovery is `dynamic_cast` on the plugin object, next to `INodeProvider`
 * (the two capabilities are independent — a provider without this hook is
 * skipped). Called on the thread that owns the host's scene; the object belongs
 * to the plugin and is never deleted by the host.
 */
class ISceneOpening
{
public:
    virtual ~ISceneOpening() = default;

    /**
     * @brief Decide whether @p scene can be opened in this process.
     *
     * @param scene The scene exactly as it would be passed to `load()` —
     *              `nodes`, `connections` and (when the host keeps it) `ui`.
     *              Not modified by the host afterwards on the strength of this
     *              call; treat it as read-only.
     * @return Acceptance, or every reason it cannot be opened.
     */
    virtual SceneOpenVerdict onSceneOpening(const QJsonObject& scene) = 0;
};

} // namespace Daqster
