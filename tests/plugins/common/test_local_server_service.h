#pragma once

#include <QtTest>

/// Tests for `Daqster::LocalServerService` (REQ-SW-PL-081): one supervisor for
/// any local HTTP server.
///
/// They guard the contract — the class' job is **the life of the process**, not
/// the protocol. So there is not a single line here about models, VRAM or
/// nodes; the "server" is `local_server_stub`, ~150 lines of Qt Core that
/// answers `200 {"status":"ok"}` on the given path. No real `sd-server` /
/// `llama-server`, no GPU, no weights.
///
/// What is guarded is exactly what broke when this code existed in three
/// copies:
///
///  1. **reuse** — a second start against an already running server does **not**
///     spawn a second process (verified by counting processes, not by trust);
///  2. **ownership** — `stop()` shuts down only its own process; a reused
///     foreign one stays alive;
///  3. **clear failure** — a missing/non-executable binary and a process that
///     died after readiness both produce `failed(reason)`, not silence;
///  4. **bounded stop** — `stop()` before readiness sends no signal to a
///     process that has not exec'ed, and is idempotent (the invariant from
///     REQ-SW-PL-050).
///
/// Ports are ephemeral (a free base port plus a range of 20), not 18080–18180:
/// the tests must not touch a live server on the machine.
class TestLocalServerService : public QObject
{
    Q_OBJECT

private slots:
    /// The user's ownership file is saved and restored around each test.
    void init();
    void cleanup();

    /// Start the stub → `started(port)`, `isReady()`, `isOwned()`; `stop()` kills it.
    void startsStubAndBecomesReady();

    /// A second `start()` against a running server → reuse, **one** process.
    void secondStartReusesRunningServer();

    /// `stop()` of the reuser does not kill the foreign server.
    void stopLeavesReusedServerAlone();

    /// Missing and non-executable binary → a clear `failed`, no crash.
    void missingBinaryFailsClearly();

    /// A process that dies after readiness → `failed`, not silence.
    void crashAfterReadyIsReported();

    /// `stop()` before `started` kills nothing foreign and does not block.
    void stopBeforeStartedIsNoOp();

    /// `stop()` is idempotent (including on a never started instance).
    void stopIsIdempotent();

    /// A taken port is skipped by the auto-selection.
    void pickFreePortSkipsBusyPort();

private:
    bool m_hadOwnershipFile = false;
    QByteArray m_ownershipBackup;
};
