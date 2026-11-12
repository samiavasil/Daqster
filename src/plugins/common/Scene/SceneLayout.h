#pragma once

// SPDX-License-Identifier: MIT
//
// SceneLayout — the one root every external artifact lives under
// (REQ-SW-PL-091, contract documented in `docs/Architecture/dependency-layout.md`).
//
// A scene says `binaries/piper` (a role) or `${DAQSTER_DEPS}/models/…`, and the
// **machine** says where that root is. Today every consumer joined its own
// subpath (`$DEPS/whisper.cpp/build/bin/whisper-server`, `$LLAMA/bin/…`), so a
// second consumer was free to join a slightly different one. This class is the
// single answer:
//
//     <DEPS>/llama.cpp/build/bin/{llama-server,llama-mtmd-cli,llama-tts}
//     <DEPS>/whisper.cpp/build/bin/whisper-server
//     <DEPS>/piper/piper            (+ espeak-ng-data next to it)
//     <DEPS>/piper-voices/<voice>.onnx (+ .onnx.json)
//     <DEPS>/sd-server
//     <DEPS>/models/{llm,asr,vlm,embedding,tts,sd}/…
//
// **One root, kinds are subdirectories.** There is no second token per kind:
// `modelsDirFor("llm")` is the answer and a new kind is a subdirectory, not a
// new environment variable. `DAQSTER_LLAMA` and `DAQSTER_LLAMA_MODELS` are
// *derived* from this tree (see `SceneVariables`) — they stay usable for the
// `.flow` files written before the layout existed, but they are no longer
// something a machine must configure.
//
// **Entries may be symlinks**, on purpose: `llama.cpp/build` is built in place
// outside the dependency root, and the weights may live on another partition
// (`fetch.sh` has a size rule). The getters therefore answer about *paths*, not
// about what exists — a symlink that is broken must not silently turn the
// answer into "not configured"; the caller checks existence and says which file
// is missing (the convention the run scripts already follow).
//
// Header-only and I/O-free: joining two strings must not cost a `stat()` per
// token lookup, and the same answer must hold for a test that never writes the
// tree.

#include <QDir>
#include <QString>
#include <QStringList>

namespace Daqster {

/**
 * @brief The dependency tree of one machine: a root plus the documented joins.
 *
 * Construct it from a root (`SceneLayout("/opt/daqster-deps")`) when the root
 * is already known — a script argument, a test, a `ModelCatalog` folder — or
 * ask the machine with `fromMachine()`.
 */
class SceneLayout
{
public:
    /// @param depsRoot The `<DEPS>` root; surrounding whitespace is ignored.
    explicit SceneLayout(const QString& depsRoot = QString());

    /**
     * @brief The layout of this machine: `DAQSTER_DEPS` from config, then env.
     *
     * @param reason Optional; why the root is empty (`DAQSTER_DEPS` unset).
     *        An unset root is **empty**, never `/<subpath>`: a silently
     *        invented absolute root is the failure mode this class prevents.
     * @return A layout whose `isValid()` is false when no root is configured.
     */
    static SceneLayout fromMachine(QString* reason = nullptr);

    /// The configured `<DEPS>` root, or an empty string.
    QString depsRoot() const { return m_depsRoot; }

    /// True when a root is configured; every getter below is empty otherwise.
    bool isValid() const { return !m_depsRoot.isEmpty(); }

    /// `<DEPS>/llama.cpp/build` — what `DAQSTER_LLAMA` derives to (`.flow`s
    /// still write `${DAQSTER_LLAMA}/bin/llama-tts`).
    QString llamaBuildRoot() const;

    /// `<DEPS>/llama.cpp/build/bin` — the directory holding the llama.cpp CLIs.
    QString llamaBinDir() const;

    /// `<DEPS>/models` — the parent of every kind directory.
    QString modelsDir() const;

    /**
     * @brief `<DEPS>/models/<kind>`.
     *
     * @param kind One of kinds() (`llm`, `asr`, `vlm`, `embedding`, `tts`,
     *        `sd`). An empty or unknown kind returns an empty string: joining
     *        an invented name would hide the typo until the load fails.
     * @return The kind directory, or an empty string.
     */
    QString modelsDirFor(const QString& kind) const;

    /// `<DEPS>/piper/piper` — the binary; `espeak-ng-data` lives beside it.
    QString piperBin() const;

    /// `<DEPS>/piper-voices` — `<voice>.onnx` plus its `.onnx.json`.
    QString piperVoicesDir() const;

    /// `<DEPS>/whisper.cpp/build/bin/whisper-server`.
    QString whisperBin() const;

    /// `<DEPS>/sd-server`.
    QString sdServer() const;

    /// The model kinds the layout defines (contract, not discovery).
    static QStringList kinds();

private:
    /// Join @p relative under the root; empty when there is no root.
    QString join(const QString& relative) const;

    QString m_depsRoot;
};

} // namespace Daqster
