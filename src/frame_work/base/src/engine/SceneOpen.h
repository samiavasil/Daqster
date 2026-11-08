#pragma once

#include "framework_core_export.h"

#include "capabilities/ISceneOpening.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QString>
#include <QVector>

#include <vector>

namespace Daqster {

class INodeProvider;

/**
 * @brief What the pre-open check found — for logs, REST replies and tests.
 *
 * The open half of the scene lifecycle (REQ-SW-PL-082). A host asks the plugins
 * *before* the graph is loaded; this report says how many of them were actually
 * asked and what they answered, so a host logs one line and a test asserts on
 * the outcome without parsing logs.
 *
 * An empty `problems` list is acceptance — including the case where no plugin
 * implements the hook at all. A scene nobody objects to must open exactly as it
 * did before the hook existed.
 */
struct SceneOpenReport {
    /// Providers that implement `ISceneOpening` and were asked. Providers
    /// without the hook are not counted: asked != discovered.
    int providersAsked = 0;
    /// Every problem every asked provider reported, in provider order. Not
    /// truncated to the first: the user should see everything at once.
    QVector<SceneOpenProblem> problems;

    /// True when the scene can be opened (also when nothing was asked).
    bool accepted() const { return problems.isEmpty(); }

    /// The problems as JSON (`nodeId`, `nodeName`, `message`, `hint`), for a
    /// REST error body or a host that marks the offending nodes.
    QJsonArray problemsJson() const;
};

/**
 * @brief Ask every provider that implements `ISceneOpening` about @p scene,
 *        telling them where the scene came from.
 *
 * Must run **before** `load()` — before the outgoing scene is stopped and
 * before its graph is destroyed — so a refusal leaves a running scene intact.
 * Providers that do not implement the hook are skipped without error (it is
 * optional); null entries are skipped as well.
 *
 * The call is a pure query by contract: it must not be a place where a plugin
 * starts processes, threads or models (see `ISceneOpening`). Asking twice with
 * the same scene must give the same answer — hosts do exactly that, because
 * `loadFlow()` checks before tearing anything down and `applySceneJson()` checks
 * again right before `load()`.
 *
 * The two-argument virtual is **always** called; a provider that only
 * implemented the single-argument one receives the scene through the default
 * implementation, so older plugins answer exactly as before.
 *
 * @param scene The scene object as it would be handed to `load()`.
 * @param scenePath Path of the `.flow` file the scene was read from, or an
 *                  empty string when it does not come from a file (an in-memory
 *                  `reloadFlow(QJsonObject)`, a REST body). Plugins use it to
 *                  find a `project.json` next to the scene.
 * @param providers Node providers, typically `QPluginManager::nodeProviders()`.
 * @return How many were asked and everything they reported.
 */
FRAMEWORK_CORE_EXPORT SceneOpenReport
checkSceneOpening(const QJsonObject& scene, const QString& scenePath,
                  const std::vector<INodeProvider*>& providers);

/**
 * @brief `checkSceneOpening()` against the process-wide plugin manager,
 *        carrying the scene's file path.
 *
 * The hosts use this overload; it is the same probe `notifySceneClosed()` uses,
 * so "who is asked" cannot differ between the close and the open half.
 *
 * @param scene The scene object as it would be handed to `load()`.
 * @param scenePath Path of the `.flow` file the scene was read from, or empty
 *                  for an in-memory scene.
 * @return How many were asked and everything they reported (empty report when
 *         no plugin manager exists).
 */
FRAMEWORK_CORE_EXPORT SceneOpenReport checkSceneOpening(const QJsonObject& scene,
                                                       const QString& scenePath);

/**
 * @brief Ask every provider that implements `ISceneOpening` about @p scene.
 *
 * Kept for hosts and plugins that have no path to pass (the graph editor's
 * in-memory operations, tests); it calls the path-carrying overload with an
 * empty path, which is the documented "scene is not a file" signal.
 *
 * @param scene The scene object as it would be handed to `load()`.
 * @param providers Node providers, typically `QPluginManager::nodeProviders()`.
 * @return How many were asked and everything they reported.
 */
FRAMEWORK_CORE_EXPORT SceneOpenReport
checkSceneOpening(const QJsonObject& scene, const std::vector<INodeProvider*>& providers);

/**
 * @brief `checkSceneOpening()` against the process-wide plugin manager.
 *
 * The hosts use this overload; it is the same probe `notifySceneClosed()` uses,
 * so "who is asked" cannot differ between the close and the open half.
 *
 * @param scene The scene object as it would be handed to `load()`.
 * @return How many were asked and everything they reported (empty report when
 *         no plugin manager exists).
 */
FRAMEWORK_CORE_EXPORT SceneOpenReport checkSceneOpening(const QJsonObject& scene);

/**
 * @brief Host-facing message for a refused scene, or an empty string.
 *
 * One place builds the text the log, the message box and the REST error body
 * show, so the user reads the same explanation wherever the load was refused —
 * the counterpart of `unsupportedHeadlessMessage()` in `HeadlessEngine.cpp`.
 *
 * One line per problem: `"<nodeName> (<nodeId>): <message> — <hint>"`, with the
 * parts that are empty left out (`"<nodeName>: <message>"` when there is no id,
 * `"<message>"` when neither is known). A host that can mark nodes uses
 * `SceneOpenReport::problemsJson()` instead of parsing this text.
 *
 * @param report The result of `checkSceneOpening()`.
 * @return Empty when accepted; otherwise the problems, one per line.
 */
FRAMEWORK_CORE_EXPORT QString sceneOpeningMessage(const SceneOpenReport& report);
} // namespace Daqster
