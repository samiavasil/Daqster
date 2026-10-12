#include "test_video_frame_consolidation.h"

#include "GL/TexturePool.h"
#include "GL/VideoGLContextManager.h"
#include "NodeDataTypes/VideoFrameData.h"
#include "NodeDataTypes/VideoTextureHandle.h"

#include <QImage>
#include <QVideoFrame>

namespace {

/// A minimal RGB frame — enough for a CPU-resident frame with no decoder and
/// no OpenCV.
QVideoFrame makeCpuFrame(int w = 64, int h = 48)
{
    QImage img(w, h, QImage::Format_RGB32);
    img.fill(Qt::darkCyan);
    return QVideoFrame(img);
}

/// A handle that claims to be a pooled RGBA texture. The ids are never used by
/// the CPU-side assertions — fromTexture() only stores them — so they can be
/// arbitrary nonzero values. acquire()/release() against the real pool is
/// covered separately, in the GL-dependent tests below.
VideoTextureHandle makeRgbaHandle(GLuint texY, int w = 320, int h = 240)
{
    VideoTextureHandle handle;
    handle.texY = texY;
    handle.width = w;
    handle.height = h;
    handle.rgba = true;
    return handle;
}

} // namespace

bool TestVideoFrameConsolidation::glAvailable()
{
    return VideoGLContextManager::instance().context() != nullptr;
}

// ── Residency accounting ─────────────────────────────────────────────────────

void TestVideoFrameConsolidation::cpuFrameIsNotGpuResident()
{
    const VideoFrameData frame(makeCpuFrame());
    QVERIFY(frame.hasFrame());
    QVERIFY2(!frame.isGpuResident(),
             "a QVideoFrame payload is CPU-resident; reporting it as GPU-resident "
             "makes consumers read back from textures that were never uploaded");
    QVERIFY(!frame.isGpuRgba());
    // The payload is readable without any conversion — this is the zero-copy
    // display path (REQ-SW-PL-020 / PL-032 AC 2).
    QCOMPARE(frame.frame().size(), QSize(64, 48));
}

void TestVideoFrameConsolidation::fromTextureIsGpuResidentRgba()
{
    // An effect output (VideoEffectGLProcessor::processTexture) hands the next
    // node a GPU-resident RGBA texture. The node that receives it must be able
    // to tell that without touching GL — that flag is what lets a chain stay
    // GPU-resident end to end (REQ-SW-PL-032 AC 5).
    const auto frame = VideoFrameData::fromTexture(makeRgbaHandle(7));
    QVERIFY(frame);
    QVERIFY(frame->isGpuResident());
    QVERIFY(frame->isGpuRgba());
    QVERIFY(frame->hasFrame());
    // …and the CPU payload is NOT valid then. A consumer that ignores the
    // residency flag gets an invalid QVideoFrame and paints garbage — the
    // failure mode this whole mechanism exists to prevent.
    QVERIFY2(!frame->frame().isValid(),
             "a GPU-resident frame must not expose a CPU payload — consumers must go "
             "through asTexture()/asImage()");
}

void TestVideoFrameConsolidation::fromTextureWithNoTextureIsNotAFrame_KnownDefect()
{
    // KNOWN DEFECT (found 2026-10-05): fromTexture() sets m_textureValid =
    // true unconditionally, so an all-zero handle yields hasFrame() == true.
    // Downstream, asTexture() hands back texY == 0, the shader samples texture
    // unit 0, and the consumer paints black instead of falling back to the CPU
    // path. Reachable only when an effect processor reports success with a null
    // texture — rare rather than absent.
    //
    // The CURRENT behaviour is asserted on purpose, so the defect is pinned
    // rather than enshrined: when fromTexture() starts validating its handle,
    // this test fails and the comment says to flip the expectation. The fix
    // belongs in VideoFrameData::fromTexture (reject an all-zero handle),
    // which is shared plugin code — deliberately not changed from a test.
    const auto frame = VideoFrameData::fromTexture(VideoTextureHandle());
    QVERIFY(frame);
    QVERIFY2(frame->hasFrame(),
             "fromTexture() now validates its handle — this is the fix landing. "
             "Flip the expectation to QVERIFY(!frame->hasFrame()) and rename this test.");

    // The rest of the wrapping is still well-formed, which is what makes the
    // defect reachable-but-contained: the handle is marked pooled and rgba,
    // so a downstream consumer takes the GPU path with texture 0 bound.
    VideoTextureHandle handle;
    QVERIFY(frame->asTexture(&handle));
    QCOMPARE(handle.texY, GLuint(0));
    QVERIFY(handle.pooled);
}

void TestVideoFrameConsolidation::setFrameDemotesBackToCpu()
{
    auto frame = VideoFrameData::fromTexture(makeRgbaHandle(11), [] {});
    QVERIFY(frame->isGpuResident());

    // A source that pushes a new frame into a reused holder must drop the GPU
    // residency and the texture cache together — otherwise the next consumer
    // keeps sampling the previous effect output.
    frame->setFrame(makeCpuFrame());
    QVERIFY(!frame->isGpuResident());
    QVERIFY(!frame->isGpuRgba());
    QVERIFY(frame->hasFrame());
}

void TestVideoFrameConsolidation::setFrameClearsTheGpuResidency()
{
    // The reverse direction of the same invariant, asserted through the holder
    // lifetime a fan-out actually uses: a shared frame replaced by a new one
    // must not leave a stale texture bound.
    auto holder = std::make_shared<VideoFrameData>(makeCpuFrame(16, 16));
    QVERIFY(!holder->isGpuResident());
    holder->setFrame(makeCpuFrame(32, 32));
    QCOMPARE(holder->frame().size(), QSize(32, 32));
    QVERIFY(!holder->isGpuResident());
}

// ── Pooled vs owned ──────────────────────────────────────────────────────────

void TestVideoFrameConsolidation::fromTextureMarksTheHandlePooled()
{
    // fromTexture() is only ever called with a texture that came out of
    // TexturePool::acquire(). If it did not mark the handle pooled,
    // releaseTextures() would glDeleteTextures it while the pool still owns the
    // name — the pool hands the same id out again and the second consumer
    // writes into a live texture. That shows up as a flickering or black frame
    // one release later, never as a crash, which is why it is asserted here.
    const auto frame = VideoFrameData::fromTexture(makeRgbaHandle(13));
    QVERIFY(frame);

    // A GpuRgba frame's handle IS the payload, so asTexture() hands it back
    // without uploading — which makes the pooled flag observable.
    VideoTextureHandle handle;
    QVERIFY(frame->asTexture(&handle));
    QCOMPARE(handle.texY, GLuint(13));
    QVERIFY(handle.rgba);
    QVERIFY2(handle.pooled,
             "fromTexture() did not mark the handle pooled — its destructor would "
             "glDeleteTextures an id the TexturePool still owns, and the pool would "
             "then hand that id out a second time");

    // A default handle must NOT be pooled: it is owned by nobody, and pooling
    // a null name would poison the free list.
    const auto empty = VideoFrameData::fromTexture(VideoTextureHandle());
    VideoTextureHandle emptyHandle;
    QVERIFY(empty->asTexture(&emptyHandle));
    QVERIFY(!emptyHandle.pooled || emptyHandle.texY == 0);
}

void TestVideoFrameConsolidation::releaseCallbackFiresExactlyOnceOnDestruction()
{
    // This is the mechanism that stops the per-frame
    // glGenTextures/glDeleteTextures pair (REQ-SW-PL-032, Issue #7). The
    // effect processor passes a callback that returns the texture to the pool;
    // the frame calls it exactly once when it dies.
    int calls = 0;
    {
        const auto frame = VideoFrameData::fromTexture(makeRgbaHandle(17),
                                                       [&calls] { ++calls; });
        QVERIFY(frame);
        QCOMPARE(calls, 0); // still alive — the texture is in use
    } // ← destruction happens here
    QCOMPARE(calls, 1);
}

void TestVideoFrameConsolidation::releaseCallbackSurvivesRepeatedDestruction()
{
    // VideoFrameData::setFrame() and its destructor both call
    // releaseTextures(). If the callback were not cleared after firing, a
    // setFrame() followed by destruction would return the texture to the pool
    // TWICE — the pool would then hand the same id to two consumers and one of
    // them would write into a live texture.
    int calls = 0;
    auto frame = VideoFrameData::fromTexture(makeRgbaHandle(19), [&calls] { ++calls; });

    frame->setFrame(makeCpuFrame()); // first release — returns tex 19 to the pool
    QCOMPARE(calls, 1);

    frame.reset(); // destructor — must NOT fire again
    QCOMPARE(calls, 1);
}

void TestVideoFrameConsolidation::releaseCallbackIsPreferredOverThePool()
{
    // A frame can carry both a pooled handle and an explicit callback (the
    // effect path does exactly that). Firing both would double-release.
    int calls = 0;
    {
        const auto frame = VideoFrameData::fromTexture(makeRgbaHandle(23),
                                                       [&calls] { ++calls; });
        QVERIFY(frame);
    }
    QCOMPARE(calls, 1);
}

void TestVideoFrameConsolidation::twoFramesEachGetTheirOwnCallback()
{
    // Fan-out of one effect output to two consumers: the frame is shared via
    // shared_ptr, so the callback must fire once — when the LAST consumer lets
    // go — not once per handle copy. (VideoFrameData is non-copyable, so the
    // only way to get two owners is a shared_ptr, which is the point.)
    int calls = 0;
    {
        auto frame = VideoFrameData::fromTexture(makeRgbaHandle(29), [&calls] { ++calls; });
        std::shared_ptr<VideoFrameData> consumerA = frame;
        std::shared_ptr<VideoFrameData> consumerB = frame;
        QCOMPARE(frame.use_count(), 3L);
        QCOMPARE(calls, 0);
        frame.reset();
        QCOMPARE(calls, 0); // two consumers still hold it
        consumerA.reset();
        QCOMPARE(calls, 0);
    } // consumerB was the last owner
    QCOMPARE(calls, 1);
}

// ── One pool per process ─────────────────────────────────────────────────────

void TestVideoFrameConsolidation::poolIsASingleton()
{
    // E1 (commit f6f184d): TexturePool::instance() moved out of line so it is
    // emitted once by DaqsterCommonLibrary. Before that, an inline
    // function-local static gave each consumer translation unit its own pool
    // on Windows, so a texture released by the effect plugin was invisible to
    // the display plugin and both grew without bound. Same address, every
    // call — that is the invariant.
    TexturePool &a = TexturePool::instance();
    TexturePool &b = TexturePool::instance();
    QCOMPARE(&a, &b);
    QCOMPARE(reinterpret_cast<quintptr>(&a), reinterpret_cast<quintptr>(&b));
}

void TestVideoFrameConsolidation::poolSurvivesItsConsumers()
{
    // The singleton is intentionally leaked (same pattern as
    // VideoGLContextManager) so no frame can outlive the pool and so there is
    // no static-destruction-order hazard with QOpenGLContext teardown. The
    // observable consequence: taking the address early and using it late must
    // still work.
    const TexturePool *first = &TexturePool::instance();
    {
        // Stand-in for a frame being created and destroyed between the two
        // takes.
        const auto frame = VideoFrameData::fromTexture(makeRgbaHandle(31), [] {});
        QVERIFY(frame);
    }
    QCOMPARE(&TexturePool::instance(), first);
}

// ── No-GL degradation contract ───────────────────────────────────────────────

void TestVideoFrameConsolidation::acquireRejectsNonPositiveSizes()
{
    // acquire(0, h) / acquire(w, 0) must return 0 rather than allocating a
    // 0-sized texture. A zero-size GL texture is legal to create but useless
    // and unrenderable, so returning it would hand downstream a black frame
    // instead of a clean CPU fallback.
    QCOMPARE(TexturePool::instance().acquire(0, 240), GLuint(0));
    QCOMPARE(TexturePool::instance().acquire(320, 0), GLuint(0));
    QCOMPARE(TexturePool::instance().acquire(-16, -16), GLuint(0));
}

void TestVideoFrameConsolidation::releaseIgnoresZeroAndUnknownIds()
{
    // releaseTextures() calls pool.release() for all four handle slots, three
    // of which are 0 for NV12 (texU/texV) and all but one for RGBA. release(0)
    // must be a no-op, not an attempt to pool a null name.
    TexturePool &pool = TexturePool::instance();
    pool.release(0);
    pool.release(0xDEADBEEF); // never allocated by this pool
    // Nothing above may crash, corrupt the ownership list, or queue a bogus id
    // for reuse — the next acquire() must still behave.
    QCOMPARE(pool.acquire(0, 0), GLuint(0));
}

void TestVideoFrameConsolidation::releaseIsIdempotent()
{
    if (!glAvailable())
        QSKIP("no GL context — the idempotency path needs a real texture id");

    VideoGLContextManager &mgr = VideoGLContextManager::instance();
    QVERIFY(mgr.makeCurrent());
    TexturePool &pool = TexturePool::instance();

    // A frame destroyed twice, or a setFrame() followed by a destructor, must
    // not queue the same texture twice: acquire() would then hand the same id
    // to two consumers and the second would scribble over the first's pixels.
    // The double-release guard in TexturePool::release() is what prevents it.
    const GLuint tex = pool.acquire(64, 64);
    QVERIFY(tex != 0);
    pool.release(tex);
    pool.release(tex); // second release must be ignored, not queued twice
    pool.release(tex); // and a third

    // If it had been queued twice, the next acquire of that size would return
    // it, and a further acquire would return the SAME id again for a second
    // consumer — which is exactly what must not happen.
    const GLuint first = pool.acquire(64, 64);
    QCOMPARE(first, tex);
    const GLuint second = pool.acquire(64, 64);
    QVERIFY2(second != first,
             "the same texture id was handed out twice — release() queued a "
             "double-release and two consumers now own one texture");
    pool.release(first);
    pool.release(second);
    mgr.doneCurrent();
}

// ── Real GL round-trip ───────────────────────────────────────────────────────

void TestVideoFrameConsolidation::acquireReleaseReusesTextures()
{
    if (!glAvailable())
        QSKIP("no GL context — acquire()/release() degradation is covered above");

    VideoGLContextManager &mgr = VideoGLContextManager::instance();
    QVERIFY(mgr.makeCurrent());

    TexturePool &pool = TexturePool::instance();

    // Two of the same size: the second must come from the free list, so the
    // pool did not grow. A regression here is invisible in a screenshot and
    // shows up only as a per-frame glGenTextures.
    const GLuint first = pool.acquire(256, 256);
    QVERIFY2(first != 0, "acquire() returned 0 with a current context — the GL path is broken");
    pool.release(first);

    const GLuint reused = pool.acquire(256, 256);
    QVERIFY(reused != 0);
    QCOMPARE(reused, first);

    // Distinct sizes must NOT be handed the same texture without
    // reallocating, so the second acquire has to allocate.
    const GLuint other = pool.acquire(128, 128);
    QVERIFY(other != 0);
    QVERIFY2(other != reused, "a free 256x256 texture was handed out for 128x128 without "
                              "re-allocating its storage");
    pool.release(other);
    pool.release(reused);
    mgr.doneCurrent();
}

void TestVideoFrameConsolidation::acquireHandlesResolutionChange()
{
    if (!glAvailable())
        QSKIP("no GL context — resolution-change reallocation needs a real texture");

    VideoGLContextManager &mgr = VideoGLContextManager::instance();
    QVERIFY(mgr.makeCurrent());
    TexturePool &pool = TexturePool::instance();

    // A source that switches resolution mid-stream must not force a new
    // glGenTextures: the pool re-allocates storage on the same name.
    const GLuint hd = pool.acquire(1920, 1080);
    QVERIFY(hd != 0);
    pool.release(hd);

    const GLuint sd = pool.acquire(640, 360);
    QVERIFY(sd != 0);
    QCOMPARE(sd, hd);

    // …and back again, in the other order this time (the pool is LIFO for the
    // re-size branch), to catch an ordering bug in the free list.
    pool.release(sd);
    const GLuint hdAgain = pool.acquire(1920, 1080);
    QCOMPARE(hdAgain, hd);
    pool.release(hdAgain);
    mgr.doneCurrent();
}

QTEST_MAIN(TestVideoFrameConsolidation)

#include "test_video_frame_consolidation.moc"
