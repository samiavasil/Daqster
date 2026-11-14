#pragma once

#include <QtTest>

/// Tests for `Daqster::DependencyManager` (REQ-SW-PL-088) — the declarative
/// manifest/install/adopt/lockfile primitive.
///
/// Everything here is local and deterministic: no network is used. The HTTPS
/// download path is reached only through its **validation** half (non-https
/// URLs are rejected before any request), while success paths are exercised
/// via `adopt()` and a manifest-preplaced file. A real `https://` peer cannot
/// be faked without certificate setup, so it is not attempted.
class TestDependencyManager : public QObject
{
    Q_OBJECT

private slots:
    /// A complete manifest parses and every field survives.
    void manifestParses();
    /// `http://` and `file://` are rejected with an error mentioning https.
    void manifestRejectsNonHttps();
    /// A download without sha256 is rejected.
    void manifestRejectsMissingChecksum();
    /// Duplicate ids and unknown kinds are rejected.
    void manifestRejectsDuplicateIdAndUnknownKind();
    /// A malformed sha256 (length/uppercase) is rejected.
    void manifestRejectsMalformedSha256();
    /// Correct sha256 → present, no error.
    void statusAcceptsCorrectChecksum();
    /// Changed content → present == false, error names the id.
    void statusDetectsChecksumMismatch();
    /// Missing file → error names the id and the expected path.
    void statusMissingNamesId();
    /// `adopt()` records the absolute path in `deps.lock.json`, no copy.
    void adoptRecordsLockWithoutCopying();
    /// `adopt()` refuses a checksum mismatch and leaves the lock untouched.
    void adoptRejectsChecksumMismatch();
    /// Adopt-only spec (no url, no sha256) records the computed checksum.
    void adoptOnlyRecordsComputedChecksum();
    /// `install()` rejects a non-https URL before any request/partial file.
    void installRejectsNonHttps();
    /// `install()` rejects a spec whose sha256 is absent.
    void installRejectsMissingChecksum();
    /// A second manager reads the lockfile written by the first.
    void lockRoundTripAcrossInstances();
};
