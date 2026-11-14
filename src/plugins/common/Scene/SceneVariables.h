#pragma once

// SPDX-License-Identifier: MIT
//
// SceneVariables — `${VAR}` resolution for scenes (REQ-SW-PL-091).
//
// A `.flow` file in git must not carry `/mnt/Builder/…` or `C:\Users\…`: the
// path is true on exactly one machine and the scene is dead everywhere else.
// The portable form is a token — `${DAQSTER_DEPS}/ggml-small.bin` — and the
// **node that consumes the path** expands it while loading. Nothing upstream
// (the loader, the IDE, the runtime host) needs to know which keys are paths.
//
// Where the values come from, in order (first hit wins):
//
//   1. **overrides** — passed by the caller of `expandDetailed()` (a test, or a
//      node that was configured programmatically);
//   2. **machine config** — group `paths` of one JSON document per machine
//      (see `defaultFilePath()`), e.g. `paths/DAQSTER_DEPS`;
//   3. **process environment** — `qgetenv(name)`;
//   4. **derived from the layout** — `DAQSTER_LLAMA` and
//      `DAQSTER_LLAMA_MODELS` are answered by `SceneLayout` out of
//      `DAQSTER_DEPS` (one root, see `docs/Architecture/dependency-layout.md`);
//   5. **empty** — the token cannot be resolved.
//
// Rule 4 is a **fallback**, not a replacement: an explicit value at any of the
// levels above still wins, so every scene written before the layout existed
// keeps resolving exactly as it did. A machine that configures only
// `DAQSTER_DEPS` no longer has to configure the two derived names — and a
// machine that configures neither gets nothing, not `/<subpath>`.
//
// An empty value counts as *not resolved* at every level: `${DEPS}/x.bin`
// with an empty `DEPS` would silently become `/x.bin`, a path that exists only
// in theory. Whoever wants a fallback writes `${DEPS:-/tmp}`.
//
// **Relative paths (REQ-SW-PL-091, second half).** A token is not the only
// portable form: a scene may carry a file that lives *next to it*
// (`"filePath": "bars_h265_1080p25_x3.mp4"`). `expand()` alone cannot express
// that, because a relative path handed to the platform resolves against the
// process cwd. The host that reads the `.flow` therefore records its directory
// (`adoptSceneFile()`), and `resolvePath()` — not `expand()` — is what a node
// calls for a path field: tokens first, then the scene's directory for a
// relative result. No scene directory and no token means no answer: empty plus
// a reason, never the cwd.
//
// Syntax (deliberately small — it is what `.flow` files use, nothing more):
//
// | Input                              | Result                    |
// |------------------------------------|---------------------------|
// | `${X}` and `X` is set, non-empty   | the value                 |
// | `${X}` and `X` is unset/empty      | the literal `${X}` + token reported |
// | `${X:-default}`                    | `default` when X unset/empty |
// | `${X:?задай X}`, X set            | the value                 |
// | `${X:?задай X}`, X unset/empty     | empty string + token **and** message reported |
// | `$$`                               | `$` (the only literal `$`)  |
// | no `$`                             | unchanged (fast path)      |
//
// There is no recursion (a default is not re-expanded) and no `$VAR` without
// braces: both are extra ways for a path to break quietly. The two rules that
// differ from a shell are on purpose — an unresolved `${X}` stays **visible**
// in the GUI and in `save()` instead of turning into "", so the user sees which
// token is missing rather than "the path is not set"; and `${X:?msg}` does not
// abort the load (this class cannot know who calls it), it reports the token
// and the message so the caller turns them into a node error.
//
// **Test/override root:** `DAQSTER_SCENE_VARIABLES` (a file path) wins over
// both candidates of `defaultFilePath()`, so a test never reads the real
// `~/.config`.

#include "DaqsterCommonExport.h"

#include <QHash>
#include <QMap>
#include <QMutex>
#include <QString>
#include <QStringList>

namespace Daqster {

/**
 * @brief Result of expanding one string.
 *
 * `text` is always usable as text (expansion never throws and never blocks a
 * load). `token`/`name` are set only when something could not be resolved —
 * that is the caller's cue to show an error that names the missing token
 * instead of complaining about an empty path.
 */
struct DAQSTERCOMMON_EXPORT SceneVariableExpansion
{
    QString text;     ///< expanded value
    QString token;    ///< `"${NAME}"` of the first unresolved expression, else empty
    QString name;     ///< `"NAME"` (no braces) — for messages and the config key
    QString message;  ///< text after `:?` of that expression, else empty

    /// True when every `${...}` was resolved (an empty `token` means so).
    bool isComplete() const { return token.isEmpty(); }
};

/**
 * @brief The `${VAR}` resolver: one token table per machine, one instance per
 *        process.
 *
 * **Thread contract:** `expand()`, `expandDetailed()`, `resolvePath()`,
 * `sceneDirectory()`, `pathToken()` and `pathTokens()` may be called from any
 * thread — they take one mutex, copy the table (or the directory) and release
 * it before parsing, so no lock is held while the caller's code runs.
 * `setPathToken()`/`removePathToken()` take the same mutex and
 * rewrite the file atomically (`QSaveFile`), so a crash never leaves a half
 * table. Re-reads are stamp-checked (mtime + size), which makes a table written
 * by **another process** (IDE ↔ NodeRunner) visible without a restart, at the
 * cost of one `stat()` per lookup — lookups happen at node load, not per frame.
 */
class DAQSTERCOMMON_EXPORT SceneVariables
{
public:
    /// Values passed by the caller; they beat config and environment.
    using Overrides = QHash<QString, QString>;

    /// The process-wide table (function-local static: thread-safe init).
    static SceneVariables& instance();

    /**
     * @brief Path of the machine-wide token document.
     *
     * `DAQSTER_SCENE_VARIABLES` (env, a file path) wins. Otherwise the first
     * candidate that already holds a file, and if there is none, the
     * launcher-independent one — the same rule the private `SettingsStore`
     * uses, and for the same reason: `ApplicationsManager` starts child apps
     * with `XDG_CONFIG_HOME=~/.config/daqster`, so `GenericConfigLocation`
     * alone would give the IDE and `NodeRunner` two different tables for one
     * machine. `~/.config/Daqster/scene-variables.json` does not depend on the
     * launcher, so it is the fallback that both sides agree on.
     *
     * On Windows there is no launcher-independent `~/.config`, so the single
     * candidate is `<GenericConfigLocation>/Daqster/scene-variables.json`.
     */
    static QString defaultFilePath();

    /**
     * @brief Token names the shipped scenes and scripts use today.
     *
     * Informational: `seedFromEnvironment()` seeds exactly these unless the
     * caller passes its own list. `DAQSTER_LLAMA` stays in the list for tables
     * written before the layout existed; it does **not** need seeding any more,
     * because it is derived from `DAQSTER_DEPS` (see the class comment).
     */
    static QStringList knownTokenNames();

    /**
     * @brief The value @p name resolves to without caller overrides.
     *
     * Machine config → process environment → derived from the layout — exactly
     * the order an expansion uses, minus rule 1. This is the "what does this
     * machine say right now" question (the settings UI, `SceneLayout`), so it
     * must not be answered by a second copy of the precedence rules.
     *
     * @param name Token name, e.g. `DAQSTER_DEPS`.
     * @return The value, or an empty string when the token is unresolved.
     */
    QString tokenValue(const QString& name) const;

    /**
     * @brief Expand every `${...}` in @p raw.
     *
     * @param raw The stored value (a `.flow` field, or an already expanded path
     *        — expansion is idempotent for results without `${`).
     * @param overrides Caller-supplied values; highest precedence.
     * @return The expanded text plus the first unresolved token, if any.
     */
    SceneVariableExpansion expandDetailed(const QString& raw,
                                          const Overrides& overrides = Overrides()) const;

    /**
     * @brief Convenience wrapper over expandDetailed() without overrides.
     *
     * @param raw The stored value.
     * @param unexpandedToken Optional; receives `"${NAME}"` (or `""`) — the same
     *        value as SceneVariableExpansion::token.
     * @return The expanded text.
     */
    QString expand(const QString& raw, QString* unexpandedToken = nullptr) const;

    /**
     * @brief Directory of the scene currently being loaded or run.
     *
     * **Ambient state**, set by the host that reads the `.flow` (`HeadlessEngine`,
     * `RuntimeShell`, `NodeEditorIdeObject`) before it hands the JSON to the
     * graph model, and read by every node that resolves a path while loading.
     * One process runs one scene at a time, so one value is enough; a host that
     * has no file (a REST body, a `reloadFlow(QJsonObject)`) leaves it alone.
     *
     * It is a directory, not a file path: a node only needs "next to what".
     * Lexically normalised (`..` folded), never canonicalised — the scene's file
     * may live behind a symlink and the stored paths are relative to the
     * *written* location.
     *
     * @return The directory, or an empty string when no scene was adopted.
     */
    QString sceneDirectory() const;

    /**
     * @brief Set the directory relative paths in a scene resolve against.
     *
     * @param directory A directory (an empty string clears it). Stored
     *        lexically normalised, without touching the filesystem.
     */
    void setSceneDirectory(const QString& directory);

    /**
     * @brief Adopt the directory of the scene file at @p scenePath.
     *
     * The one helper the three hosts call, so `QFileInfo::absolutePath()`
     * (and the "an in-memory scene has no directory" rule) exist once.
     *
     * @param scenePath Path of the `.flow` that was read, or empty. Empty means
     *        "this scene did not come from a file": the previous directory is
     *        kept — a hot-reload of the running scene must not turn its own
     *        relative paths into cwd-relative ones.
     */
    void adoptSceneFile(const QString& scenePath);

    /**
     * @brief Resolve a stored path: tokens first, then the scene's directory.
     *
     * `expand()` answers "which machine is this path on". It does not answer
     * "which file is this": a committed scene carries `bars.mp4` **next to
     * itself**, and a relative path handed to `QUrl::fromLocalFile()` resolves
     * against the process cwd — true only when the process happens to be
     * started in the scene's directory. So a relative result is anchored to
     * sceneDirectory() and lexically normalised; an absolute result is returned
     * untouched.
     *
     * Three answers, one per failure:
     *   - an unresolved `${X}`: the expanded text is returned **with the literal
     *     `${X}` still visible** (so `save()` round-trips it and the user sees
     *     which token is missing) and @p unexpandedToken names it;
     *   - a relative path with no scene directory: **empty** plus @p reason —
     *     the cwd is exactly what this function exists to stop being an answer;
     *   - otherwise: the resolved path (empty input stays empty and is not an
     *     error — "no file chosen" is a valid state).
     *
     * @param raw The stored value (a `.flow` field, or an already resolved path
     *        — resolution is idempotent for an absolute path without `${`).
     * @param unexpandedToken Optional; receives `"${NAME}"` (or `""`).
     * @param reason Optional; why the result is empty (empty when it is not).
     * @return The absolute path, the visible literal, or an empty string.
     */
    QString resolvePath(const QString& raw, QString* unexpandedToken = nullptr,
                        QString* reason = nullptr) const;

    /**
     * @brief The table value of @p name (config only — no overrides, no env).
     *
     * For the settings UI: "what is persisted for this token right now".
     *
     * @param name Token name, e.g. `DAQSTER_DEPS`.
     * @return The persisted value, or an empty string when absent.
     */
    QString pathToken(const QString& name) const;

    /// The whole persisted table, sorted — for the settings UI.
    QMap<QString, QString> pathTokens() const;

    /**
     * @brief Persist one token (`paths/<name>` in the shared document).
     *
     * @param name Token name; must be non-empty and must not contain `$`, `{`,
     *        `}`, `:` or `/` (it is written into `.flow` files, so a name that
     *        looks like syntax would make expansion ambiguous).
     * @param value Value; empty removes the key (same as removePathToken()).
     * @param error Optional; receives the reason on failure.
     * @return true when the document was written and the cache updated.
     */
    bool setPathToken(const QString& name, const QString& value, QString* error = nullptr);

    /**
     * @brief Remove one token; a missing key is not an error.
     *
     * @param name Token name.
     * @return true when the document could be written (or had nothing to change).
     */
    bool removePathToken(const QString& name);

    /**
     * @brief Copy known tokens from the process environment into the table.
     *
     * Only **absent** keys are copied: an existing `paths/<name>` is never
     * overwritten, so a machine that was configured once does not drift back to
     * whatever the launcher happened to export. An empty environment variable
     * is not copied (it would only shadow the config with "").
     *
     * @param names Token names to consider; empty → knownTokenNames().
     * @return How many keys were written.
     */
    int seedFromEnvironment(const QStringList& names = QStringList());

    /// Path of the document this instance reads/writes.
    QString filePath() const;

    /**
     * @brief Point this instance at another document (tests, unusual setups).
     *
     * The cache is dropped: the next lookup re-reads. Passing an empty string
     * restores `defaultFilePath()`.
     */
    void setFilePath(const QString& path);

private:
    SceneVariables();
    ~SceneVariables() = default;

    Q_DISABLE_COPY(SceneVariables)

    /// Re-read the document when it is not cached, or changed on disk.
    void ensureLoaded() const;

    mutable QMutex           m_mutex;
    mutable QMap<QString, QString> m_tokens;
    mutable QString          m_stamp;    ///< "mtime:size" of the cached file
    mutable bool             m_loaded = false;
    QString                  m_filePath;
    /// Ambient scene directory for relative paths (see resolvePath()).
    QString                  m_sceneDirectory;
};

} // namespace Daqster
