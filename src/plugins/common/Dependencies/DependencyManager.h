#pragma once

// SPDX-License-Identifier: MIT
//
// DependencyManager — declarative management of external artifacts
// (REQ-SW-PL-088).
//
// External artifacts (binaries, libraries, models, voices, sources) are
// described in ONE manifest (`dependencies.json`) instead of being hand-copied
// into scripts and environment variables. The manager does the boring half:
//
//   manifest → status → (install | adopt) → lockfile
//
// It knows **nothing** about AI, catalogs or node types: `kind` is data, and
// `provides` is informational (the catalog wiring belongs to the consumer).
//
// **Manifest format** (`dependencies.json`):
//
//   {
//     "version": 1,
//     "dependencies": [
//       {
//         "id": "piper",                 // unique; may be nested (voice/xx)
//         "kind": "binary",              // binary|library|model|voice|source
//         "version": "1.2.0",            // free form
//         "url": "https://…/p.tar.gz",   // https only; empty => adopt-only
//         "sha256": "<64 lowercase hex>",// required unless adopt-only
//         "size": 12345678,              // bytes, 0 = unknown
//         "file": "piper",               // name inside <root>/<id>/<version>/
//         "provides": ["binaries/piper"],// informational
//         "executable": true             // informational
//       }
//     ]
//   }
//
// **Install layout:** `<root>/<id>/<version>/<file>`.
// **Lockfile:** `<root>/deps.lock.json` records
// `{id, version, path, sha256, adopted}` for every installed/adopted artifact.
//
// **Rules enforced here:**
//   * `https://` only — `http://`, `file://` and bare paths are rejected with a
//     clear error (a manifest cannot turn a download into plain text);
//   * `sha256` is mandatory for anything with a `url`; it is verified before
//     the file is moved into place;
//   * install is atomic (`QSaveFile` temp → verify → rename), so a failed
//     download leaves **no** partial file;
//   * one install at a time per id;
//   * `adopt()` verifies an existing file and records it in the lockfile
//     **without copying** (the absolute path is recorded);
//   * no shell-outs and no `waitFor*()` — everything is asynchronous and
//     signal-driven.

#include "DaqsterCommonExport.h"

#include <QByteArray>
#include <QJsonObject>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QVector>

#include <functional>

class QNetworkAccessManager;

namespace Daqster {

/**
 * @brief One external artifact declared in the manifest.
 *
 * Fields mirror the JSON one-to-one; `provides` and `executable` are
 * informational and never change install/verify behaviour.
 */
struct DAQSTERCOMMON_EXPORT DependencySpec
{
    QString     id;          ///< unique id (may be nested: `voice/bg_BG-…`)
    QString     kind;        ///< `binary` | `library` | `model` | `voice` | `source`
    QString     version;     ///< free form, e.g. `0.8.0` or `8642541b2`
    QString     url;         ///< `https://…`; empty => adopt-only
    QString     sha256;      ///< 64 lowercase hex; required unless adopt-only
    qint64      size = 0;    ///< bytes, 0 = unknown
    QString     file;        ///< file name inside `<root>/<id>/<version>/`
    QStringList provides;    ///< catalog roles/kinds this satisfies (informational)
    bool        executable = false; ///< informational
};

/**
 * @brief Result of checking one artifact on disk.
 *
 * `present` is true only when the file exists **and** matches the declared
 * size/checksum. `error` is empty on success and otherwise names the id and
 * the reason (missing / size mismatch / checksum mismatch).
 */
struct DAQSTERCOMMON_EXPORT DependencyStatus
{
    bool    present = false;
    QString path;
    QString error;
};

/**
 * @brief Manifest-driven install/adopt/verify of external artifacts.
 *
 * The instance belongs to the thread that created it (it owns a
 * `QNetworkAccessManager`). All I/O is asynchronous except the small
 * manifest/lockfile reads and `adopt()`, which is called with a path that
 * already exists locally.
 */
class DAQSTERCOMMON_EXPORT DependencyManager : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief Create a manager rooted at @p rootDir.
     *
     * `rootDir` is where installed artifacts live
     * (`<rootDir>/<id>/<version>/<file>`) and where `deps.lock.json` is
     * written. An existing lockfile is read lazily by `lock()`/`status()`.
     */
    explicit DependencyManager(const QString& rootDir, QObject* parent = nullptr);

    /// Root directory of installed artifacts.
    QString rootDir() const;

    /**
     * @brief Read and validate a `dependencies.json` file.
     *
     * Validation is all-or-nothing: the manifest is rejected (and existing
     * `specs()` left untouched) when a field is missing, an id repeats, a
     * `url` is not `https://`, or a download would have no `sha256`.
     *
     * @param manifestPath Path to the JSON manifest.
     * @param error Optional; receives a human-readable reason on failure.
     * @return true when the manifest was parsed and every spec is valid.
     */
    bool loadManifest(const QString& manifestPath, QString* error);

    /// The specs of the last successfully loaded manifest.
    QVector<DependencySpec> specs() const;

    /**
     * @brief Check whether @p spec is present and matches its checksum/size.
     *
     * The path is the lockfile's recorded path when present (so an adopted
     * file outside the root is checked where it actually lives), otherwise
     * `<root>/<id>/<version>/<file>`.
     */
    DependencyStatus status(const DependencySpec& spec) const;

    /**
     * @brief Download, verify and atomically install @p spec.
     *
     * Returns immediately; `done` (and `finished`) is called once on the
     * thread's event loop. A rejected URL or checksum fails synchronously
     * through the same callback. A failed download leaves no partial file.
     */
    void install(const DependencySpec& spec,
                 std::function<void(bool ok, QString error)> done);

    /**
     * @brief Verify @p existingPath and record it in the lockfile, no copy.
     *
     * Used for migration: artifacts that already sit somewhere on disk are
     * checked against the spec and then referenced by absolute path.
     *
     * @param spec The manifest entry (checksum is required unless adopt-only).
     * @param existingPath Absolute or relative path of the existing file.
     * @param error Optional; receives the reason on failure.
     */
    bool adopt(const DependencySpec& spec, const QString& existingPath,
               QString* error);

    /// Parsed lockfile (`{"version":1,"dependencies":[…]}`).
    QJsonObject lock() const;

    /// `<rootDir>/deps.lock.json`.
    QString lockFilePath() const;

signals:
    /// Bytes received so far for @p id (`total` = 0 when unknown).
    void progress(const QString& id, qint64 received, qint64 total);

    /// Terminal result of one install (`ok == false` => @p error is set).
    void finished(const QString& id, bool ok, const QString& error);

private:
    void loadLock();
    bool writeLock(QString* error);
    bool recordInLock(const DependencySpec& spec, const QString& absolutePath,
                      bool adopted, const QString& effectiveSha256,
                      QString* error);
    QJsonObject lockEntry(const QString& id) const;
    QString expectedPath(const DependencySpec& spec) const;

    QString m_rootDir;
    QVector<DependencySpec> m_specs;
    QJsonObject m_lock;
    QNetworkAccessManager* m_nam = nullptr;
    QStringList m_installing;
};

} // namespace Daqster
