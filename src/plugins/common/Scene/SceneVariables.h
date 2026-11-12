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
//   4. **empty** — the token cannot be resolved.
//
// An empty value counts as *not resolved* at every level: `${DEPS}/x.bin`
// with an empty `DEPS` would silently become `/x.bin`, a path that exists only
// in theory. Whoever wants a fallback writes `${DEPS:-/tmp}`.
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
 * **Thread contract:** `expand()`, `expandDetailed()`, `pathToken()` and
 * `pathTokens()` may be called from any thread — they take one mutex, copy the
 * table and release it before parsing, so no lock is held while the caller's
 * code runs. `setPathToken()`/`removePathToken()` take the same mutex and
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
     * caller passes its own list.
     */
    static QStringList knownTokenNames();

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
};

} // namespace Daqster
