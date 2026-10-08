#pragma once

#include <QtTest>

/// Consolidation invariants for VideoFrameData and the global TexturePool
/// (REQ-SW-PL-032, AC 12 — and the AC 13/15/17 invariants they rest on).
///
/// REQ-SW-PL-032's whole claim is "at most one copy of a frame's
/// representation, lazy and shared". That claim has two halves, and the
/// dangerous one is invisible in a screenshot: if a frame's texture is
/// glDeleteTextures'd instead of returned to the pool, or a pool release never
/// fires, the pipeline still renders correctly and simply becomes slower —
/// one glGenTextures/glDeleteTextures pair per frame, which is exactly the cost
/// the pool exists to remove. So these tests count pool operations and
/// callback invocations rather than comparing pixels.
///
/// GL-optional by design. On a machine with a context the real acquire/release
/// path runs; without one (headless CI, offscreen) the documented degradation
/// contract runs instead: acquire() returns 0 and never a bogus id, so
/// callers fall back to the CPU path. Both are asserted — a change that makes
/// acquire() crash without GL fails here.
///
/// QTEST_MAIN provides a QGuiApplication (not QCoreApplication): the
/// GL-dependent checks construct a QOpenGLContext / QOffscreenSurface through
/// VideoGLContextManager, and Qt segfaults constructing those with no
/// QGuiApplication. The target links Qt::Gui but NOT Qt::Widgets, so QTest
/// builds a QGuiApplication and no widgets are involved.
class TestVideoFrameConsolidation : public QObject
{
    Q_OBJECT

private slots:
    // ── Residency accounting (AC 13/17) ──────────────────────────────────────
    void cpuFrameIsNotGpuResident();
    void fromTextureIsGpuResidentRgba();
    void fromTextureWithNoTextureIsNotAFrame_KnownDefect();
    void setFrameDemotesBackToCpu();
    void setFrameClearsTheGpuResidency();

    // ── Pooled vs owned (AC 17) ──────────────────────────────────────────────
    void fromTextureMarksTheHandlePooled();
    void releaseCallbackFiresExactlyOnceOnDestruction();
    void releaseCallbackSurvivesRepeatedDestruction();
    void releaseCallbackIsPreferredOverThePool();
    void twoFramesEachGetTheirOwnCallback();

    // ── TexturePool: one pool per process (E1 / AC 13) ──────────────────────
    void poolIsASingleton();
    void poolSurvivesItsConsumers();

    // ── TexturePool: the no-GL degradation contract ─────────────────────────
    void acquireRejectsNonPositiveSizes();
    void releaseIgnoresZeroAndUnknownIds();
    void releaseIsIdempotent();

    // ── TexturePool: real GL round-trip ─────────────────────────────────────
    void acquireReleaseReusesTextures();
    void acquireHandlesResolutionChange();

private:
    /// True when a shared GL context could be created in this process. Every
    /// GL-dependent assertion QSKIPs when it is false rather than passing
    /// vacuously.
    static bool glAvailable();
};
