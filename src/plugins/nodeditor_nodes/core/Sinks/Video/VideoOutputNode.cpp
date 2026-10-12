#include "VideoOutputNode.h"

#include "GL/TexturePool.h"
#include "GL/VideoGLContextManager.h"
#include "NodeDataTypes/VideoFrameData.h"
#include "PerfProfiler.h"
#include "VideoCompat.h"
#include "VideoDisplayWidget.h"
#include "VideoPerfBadge.h"

#include <algorithm>

using QtNodes::NodeData;
using QtNodes::NodeDataType;
using QtNodes::PortIndex;
using QtNodes::PortType;

VideoOutputNode::VideoOutputNode()
{
    // Display nodes must never get a graphics effect (perf): the shadow blur
    // runs per repaint and costs ~46% CPU during video playback (PERF results,
    // tests/performance/performance-video-display-2026-08-13.md).
    QtNodes::NodeStyle s = this->nodeStyle();
    s.ShadowEnabled = false;
    this->setNodeStyle(s);

    m_specs = VideoEffectOps::allSpecs();

    // Preview: fires ONCE per Play, ~1 s after the first frame. Single-shot, so
    // the preview stays static until the next Play and costs nothing ongoing.
    m_previewTimer = new QTimer(this);
    m_previewTimer->setSingleShot(true);
    m_previewTimer->setInterval(1000);
    connect(m_previewTimer, &QTimer::timeout, this, &VideoOutputNode::updatePreview);

    // 500 ms UI refresh. Runs only while the Perf toggle is on.
    m_perfRefreshTimer = new QTimer(this);
    m_perfRefreshTimer->setInterval(500);
    connect(m_perfRefreshTimer, &QTimer::timeout, this,
            &VideoOutputNode::samplePerfStats);

    // 5 s [PERF] console line (REQ-SW-PL-027). tools/measure_flow_memory.sh
    // greps for this line to confirm playback actually started.
    m_consoleTimer = new QTimer(this);
    m_consoleTimer->setInterval(5000);
    connect(m_consoleTimer, &QTimer::timeout, this, &VideoOutputNode::logPerfLine);
}

VideoOutputNode::~VideoOutputNode()
{
    // Drop the UI pointer BEFORE stop(). The UI is owned by the editor's widget
    // tree, not by this model: embedding reparents rootWidget() into the scene,
    // so the tree is free to destroy the UI first and leave this model alive
    // (a deleted node outlives its embedded widget; a scene teardown can also
    // destroy them in either order). stop() → resetVideoState() calls
    // clearPreview() and display()->clear(), so with a stale pointer here the
    // destructor dereferenced freed memory. Nulling first makes those calls
    // no-ops, and stop() still does the one job that matters here: the timers.
    m_ui = nullptr;
    stop();
}

void VideoOutputNode::stop()
{
    // Report a run that processed frames but had no UI to render into
    // (REQ-SW-PL-056 AC4). Only ever non-zero in headless mode: in the GUI the
    // factory attaches a UI before frames arrive, so the counter stays zero
    // and this stays silent. Emitted unconditionally at qInfo with no category,
    // because the log categories are disabled by default and a line the user
    // never sees is the same as no line (same reasoning as logPerfLine()).
    if (m_framesWithoutUi > 0) {
        qInfo().noquote()
            << QStringLiteral("VideoOutputNode: %1 frame(s) processed, no display "
                              "backend attached — nothing was rendered (headless)")
                   .arg(m_framesWithoutUi);
        m_framesWithoutUi = 0;
    }

    // Idempotent: stopping an already-stopped timer is a no-op.
    if (m_perfRefreshTimer != nullptr)
        m_perfRefreshTimer->stop();
    if (m_consoleTimer != nullptr)
        m_consoleTimer->stop();
    if (m_previewTimer != nullptr)
        m_previewTimer->stop();
    // Reset the fired-latch so the next Play's first frame re-arms the timer.
    m_previewFired = false;
    resetVideoState();
}

void VideoOutputNode::resetVideoState()
{
    m_lastFrameForPreview.reset();
    m_previewFired = false;
    if (m_ui != nullptr) {
        m_ui->clearPreview();
        if (m_ui->display() != nullptr)
            m_ui->display()->clear();
    }
}

VideoDisplayWidget *VideoOutputNode::displayWidget() const
{
    return m_ui != nullptr ? m_ui->display() : nullptr;
}

bool VideoOutputNode::isLiveDisplayActive() const
{
    return m_ui != nullptr && m_ui->isLiveActive();
}

QWidget *VideoOutputNode::controlsWidget() const
{
    return m_ui != nullptr ? m_ui->controlsWidget() : nullptr;
}

void VideoOutputNode::setPerfEnabled(bool enabled)
{
    if (m_ui != nullptr)
        m_ui->setPerfChecked(enabled);
}

void VideoOutputNode::forceLiveDisplay()
{
    // Placement-context live mode (REQ-SW-PL-055): the runtime layout builder
    // deembeds node widgets BY DEFINITION — there is no canvas in --run or
    // presentation mode. Deciding from Show events alone would let a missed Show
    // leave the widget on the dark "No video" preview page, so force it.
    if (m_previewTimer != nullptr)
        m_previewTimer->stop();
    // Reset the latch so a stale snapshot cannot mask live video if the widget is
    // later re-embedded into the editor.
    m_previewFired = false;
    if (m_ui != nullptr)
        m_ui->setLiveActive(true);
}

void VideoOutputNode::onUiVisibilityChanged(bool inProxy)
{
    driveDisplayMode();
    Q_UNUSED(inProxy);
}

void VideoOutputNode::driveDisplayMode()
{
    if (m_ui == nullptr)
        return;

    if (m_ui->isEmbeddedInProxy()) {
        // Editor canvas: static preview page. Arm the single-shot timer if a
        // frame has arrived and the preview has not fired yet.
        m_ui->setLiveActive(false);
        if (m_previewTimer != nullptr && !m_previewFired && !m_previewTimer->isActive()
            && m_lastFrameForPreview && m_lastFrameForPreview->hasFrame())
            m_previewTimer->start();
    } else {
        // Deembedded (floating window) or run mode (MDI): live page, no preview
        // conversion at all.
        m_ui->setLiveActive(true);
        if (m_previewTimer != nullptr)
            m_previewTimer->stop();
    }
}

void VideoOutputNode::onEffectSelectionChanged(int comboIndex)
{
    setEffectIndex(comboIndex);
    // Re-apply to the last frame so a paused source updates immediately
    // (mirrors VideoEffectNode.cpp:328-332, REQ-SW-PL-039).
    reprocessCurrentFrame();
}

void VideoOutputNode::onEffectParamsChanged()
{
    reprocessCurrentFrame();
}

void VideoOutputNode::onPerfToggled(bool enabled)
{
    auto &domain = Daqster::Perf::Domain::get("video");
    domain.setEnabled(enabled);
    if (enabled) {
        m_perfRefreshTimer->start();
        m_consoleTimer->start();
    } else {
        m_perfRefreshTimer->stop();
        m_consoleTimer->stop();
        if (m_ui != nullptr)
            m_ui->updatePerfStats(VideoPerfStats());
    }
}

void VideoOutputNode::updatePreview()
{
    if (m_ui == nullptr)
        return;

    // Visibility gate (REQ-SW-PL-055 perf fix): in standalone --run mode the node
    // widgets ARE created (loadFlow builds the scene) but the canvas is hidden,
    // so the preview label is never visible — skip the conversion entirely. The
    // UI's Hide event additionally stops the timer, so this is the second line
    // of defence.
    if (!m_ui->previewVisible())
        return;

    // No frame yet → keep the placeholder.
    if (!m_lastFrameForPreview || !m_lastFrameForPreview->hasFrame()) {
        m_ui->clearPreview();
        return;
    }

    // Scale-before-convert: convert DIRECTLY to a ~200 px preview instead of
    // materializing the full 1080p QImage and scaling afterwards. NV12 /
    // YUV420P frames are subsampled during the YUV→RGB conversion, so the full
    // image is never built and the preview costs ~(200/1920)² of a full
    // conversion. GPU-resident RGBA (effect output) and unsupported CPU formats
    // fall back to asImage() (readback — unavoidable) plus scale.
    const int kPreviewMaxWidth = 200;
    QImage image = VideoFrameData::frameToImageScaled(
        m_lastFrameForPreview->frame(), kPreviewMaxWidth);
    if (image.isNull()) {
        const QImage full = m_lastFrameForPreview->asImage();
        if (!full.isNull()) {
            const QSize target = full.size().scaled(
                QSize(kPreviewMaxWidth, kPreviewMaxWidth), Qt::KeepAspectRatio);
            image = full.scaled(target, Qt::KeepAspectRatio,
                                Qt::SmoothTransformation);
        }
    }
    if (image.isNull()) {
        m_ui->clearPreview();
        return;
    }

    m_ui->setPreviewImage(image);
    // Latch: fired for this Play. Do not re-arm on later frames.
    m_previewFired = true;
}

void VideoOutputNode::setEffectIndex(int index)
{
    // Index 0 = "No effect"; 1..N map to m_specs[0..N-1].
    if (index <= 0 || index > m_specs.size()) {
        m_effectEnabled = false;
        m_effectIndex = -1;
    } else {
        m_effectEnabled = true;
        m_effectIndex = index - 1;
    }
    if (m_ui != nullptr)
        m_ui->setEffectPage(index);
}

void VideoOutputNode::samplePerfStats()
{
    if (m_ui == nullptr)
        return;

    VideoPerfStats stats;
    auto &domain = Daqster::Perf::Domain::get("video");
    if (domain.enabled()) {
        // All perf domain values are nanoseconds (recorded by PERF_SCOPE).
        const qint64 frameIntervalNs = domain.avg("source.frame_interval");
        const qint64 presentNs = domain.avg("output.present");
        const qint64 totalNs = domain.avg("output.total");
        stats.fps = frameIntervalNs > 0 ? 1e9 / static_cast<double>(frameIntervalNs) : 0.0;
        stats.gapMs = frameIntervalNs > 0 ? static_cast<double>(frameIntervalNs) / 1e6 : 0.0;
        stats.presentMs = presentNs > 0 ? static_cast<double>(presentNs) / 1e6 : 0.0;
        stats.totalMs = totalNs > 0 ? static_cast<double>(totalNs) / 1e6 : 0.0;
        stats.cpuPercent = m_cpu.sample();

        const VideoDisplayWidget *display = m_ui->display();
        stats.hardwareGL = display != nullptr && display->isGpuBackend();
    }
    // Markers stay from the last presented frame even while the domain is off, so
    // HW/SW, format and handle are still readable with perf disabled.
    stats.handleType = m_lastHandleType;
    stats.pixelFormat = m_lastPixelFormat;
    m_ui->updatePerfStats(stats);
}

void VideoOutputNode::logPerfLine()
{
    auto &domain = Daqster::Perf::Domain::get("video");
    if (!domain.enabled())
        return;

    // Sample self-CPU first: the first sample only establishes the baseline and
    // returns 0.0 (the "cpu=0.0%" on the very first line is expected).
    // m_consoleCpu is a DEDICATED instance (not m_cpu): the 500 ms UI refresh
    // timer samples m_cpu destructively ~10x between console prints, so sharing
    // it would make this a ~0-500 ms delta (idle windows → 0) instead of a true
    // 5 s average.
    const double cpuPercent = m_consoleCpu.sample();

    // Log only once there are actual frame records.
    if (domain.count("output.total") <= 0
        && domain.count("source.frame_interval") <= 0) {
        return;
    }

    // Info level with NO category: the "daqster.perf" category is disabled by
    // default in LogManager, so a qCDebug(lcPerf) line would be filtered out and
    // the [PERF] report would never reach the console.
    qInfo().noquote()
        << formatPerfLine(domain.avg("source.frame_interval"),
                          domain.avg("output.present"),
                          domain.avg("output.total"),
                          cpuPercent,
                          m_lastHandleType, m_lastPixelFormat);
}

QJsonObject VideoOutputNode::save() const
{
    QJsonObject obj = QtNodes::NodeDelegateModel::save();

    // Effect id only while an effect is active. No-effect saves omit the key, so
    // old graphs without "effect" load as no-effect (backward compatible).
    if (m_effectEnabled && m_effectIndex >= 0 && m_effectIndex < m_specs.size())
        obj[QStringLiteral("effect")] = m_specs[m_effectIndex].id;

    obj[QStringLiteral("brightness")] = m_params.brightness;
    obj[QStringLiteral("contrast")] = m_params.contrast;
    obj[QStringLiteral("flipMode")] = m_params.flipHorizontal
        ? QStringLiteral("horizontal")
        : QStringLiteral("vertical");
    obj[QStringLiteral("blurRadius")] = m_params.blurRadius;
    obj[QStringLiteral("gaussianKernel")] = m_params.gaussianKernel;
    obj[QStringLiteral("cannyLow")] = m_params.cannyLow;
    obj[QStringLiteral("cannyHigh")] = m_params.cannyHigh;
    obj[QStringLiteral("thresholdValue")] = m_params.thresholdValue;

    // Persist the perf toggle so the domain + timers are restored across restart.
    if (m_ui != nullptr)
        obj[QStringLiteral("perfToggle")] = m_ui->perfChecked();

    return obj;
}

void VideoOutputNode::load(QJsonObject const &p)
{
    m_params.brightness = p.value(QStringLiteral("brightness")).toInt(m_params.brightness);
    m_params.contrast = p.value(QStringLiteral("contrast")).toInt(m_params.contrast);
    m_params.flipHorizontal = (p.value(QStringLiteral("flipMode")).toString()
                               != QStringLiteral("vertical"));
    m_params.blurRadius = p.value(QStringLiteral("blurRadius")).toInt(m_params.blurRadius);
    m_params.gaussianKernel = p.value(QStringLiteral("gaussianKernel")).toInt(m_params.gaussianKernel);
    m_params.cannyLow = p.value(QStringLiteral("cannyLow")).toInt(m_params.cannyLow);
    m_params.cannyHigh = p.value(QStringLiteral("cannyHigh")).toInt(m_params.cannyHigh);
    m_params.thresholdValue = p.value(QStringLiteral("thresholdValue")).toInt(m_params.thresholdValue);

    // Clamp to valid ranges (mirrors VideoEffectNode.cpp:64-70).
    m_params.brightness = std::max(-100, std::min(100, m_params.brightness));
    m_params.contrast = std::max(0, std::min(200, m_params.contrast));
    m_params.blurRadius = std::max(0, std::min(10, m_params.blurRadius));
    m_params.gaussianKernel = std::max(1, std::min(31, m_params.gaussianKernel | 1));
    m_params.cannyLow = std::max(0, std::min(255, m_params.cannyLow));
    m_params.cannyHigh = std::max(0, std::min(255, m_params.cannyHigh));
    m_params.thresholdValue = std::max(0, std::min(255, m_params.thresholdValue));

    // Effect selection: absent/empty/invalid id → no effect.
    const QString effectId = p.value(QStringLiteral("effect")).toString();
    int comboIndex = 0;   // "No effect"
    for (int i = 0; i < m_specs.size(); ++i) {
        if (m_specs[i].id == effectId) {
            comboIndex = i + 1;   // combo index = spec index + 1
            break;
        }
    }
    setEffectIndex(comboIndex);

    if (m_ui != nullptr) {
        // Restored params live in the struct the parameter widgets are bound to,
        // so rebuilding the pages is what makes the sliders show them.
        m_ui->syncEffectParams();
        // Setting checked emits toggled, which enables/disables the perf domain
        // and starts/stops the timers.
        m_ui->setPerfChecked(p.value(QStringLiteral("perfToggle")).toBool(false));
    }

    // Re-apply the restored effect/parameters to the current frame. No-op when
    // no frame has arrived yet or the input edge is not connected.
    reprocessCurrentFrame();
}

void VideoOutputNode::reprocessCurrentFrame()
{
    if (!m_lastInput || !m_lastInput->hasFrame())
        return;
    // The reprocess re-presents the last frame only while the live page is
    // active (setInData gates presentation on it). The preview is refreshed
    // inside setInData, so the user still sees the change immediately.
    setInData(m_lastInput, 0);
}

unsigned int VideoOutputNode::nPorts(PortType portType) const
{
    switch (portType) {
    case PortType::In:
        // Port 0: "video-frame" (zero-copy GPU, single video-frame type).
        return 1;
    case PortType::Out:
        return 1;
    default:
        return 0;
    }
}

NodeDataType VideoOutputNode::dataType(PortType portType, PortIndex portIndex) const
{
    Q_UNUSED(portIndex);
    return VideoFrameData().type();
}

std::shared_ptr<NodeData> VideoOutputNode::outData(PortIndex port)
{
    Q_UNUSED(port);
    return m_output;
}

void VideoOutputNode::setInData(std::shared_ptr<NodeData> data, PortIndex portIndex)
{
    if (portIndex != 0)
        return;

    // "output.total" spans the whole port-0 frame processing (REQ-SW-PL-027).
    PERF_SCOPE("video", "output.total");

    auto videoFrame = std::dynamic_pointer_cast<VideoFrameData>(data);
    if (!videoFrame || !videoFrame->hasFrame()) {
        m_lastInput.reset();
        m_output.reset();
        resetVideoState();
        Q_EMIT dataInvalidated(0);
        return;
    }

    // CRITICAL GUARD: disconnecting the edge does NOT stop the source player —
    // frames keep flowing in and would resurrect the display. The connection
    // flag (not widget nullness) is the correct guard: inputConnectionDeleted()
    // clears it and clears the display with it.
    if (!m_videoInputConnected)
        return;

    // Headless accounting (REQ-SW-PL-056 AC4): no widget provider exists in
    // headless mode, so m_ui stays null and nothing below reaches a display.
    // Counted here so stop() can say what the run did instead of reporting
    // nothing — a run that renders zero frames must not look like a run that
    // never happened.
    if (m_ui == nullptr)
        ++m_framesWithoutUi;

    // Perf markers for the panel (REQ-SW-PL-027). No-op while the domain is off.
    if (PERF_ENABLED("video")) {
        const QVideoFrame &frame = videoFrame->frame();
        m_lastHandleType = static_cast<int>(frame.handleType());
        m_lastPixelFormat = VideoCompat::pixelFormatInt(frame);
    }

    m_lastInput = videoFrame;

    // ── Embedded effect (REQ-SW-PL-034, optional, default none) ─────────────
    // When no effect is selected this block is SKIPPED ENTIRELY — asTexture() and
    // asImage() are never called and the display path below is byte-identical to
    // a node without embedded effects (zero-copy passthrough preserved).
    if (m_effectEnabled && m_effectIndex >= 0 && m_effectIndex < m_specs.size()) {
        const EffectSpec &spec = m_specs[m_effectIndex];
        // Runtime backend selection (mirrors VideoEffectNode): GpuOrCpu effects
        // run on the GPU only with hardware GL.
        const bool useGpu = (spec.backend == EffectSpec::Backend::GpuOrCpu)
            && VideoGLContextManager::hasHardwareGL();
        // Tracks whether the GPU path produced the output. When it did NOT (a
        // CpuOnly effect, no hardware GL, or a failed asTexture/processTexture)
        // the CPU path runs — INCLUDING for GpuRgba inputs, whose asImage()
        // readback mirrors VideoEffectNode.cpp:184-188 (REQ-SW-PL-039).
        bool gpuApplied = false;
        if (useGpu) {
            VideoTextureHandle input;
            if (videoFrame->asTexture(&input)) {
                VideoTextureHandle out;
                if (m_glProcessor.processTexture(input, spec, m_params, &out)) {
                    // Texture-pool path (REQ-SW-PL-032 Issue #7 / REQ-SW-PL-038):
                    // the output texture returns to the global pool when the
                    // frame dies instead of being deleted.
                    videoFrame = VideoFrameData::fromTexture(
                        out, [tex = out.texY]() {
                            TexturePool::instance().release(tex);
                        });
                    gpuApplied = true;
                }
            }
        }
        if (!gpuApplied) {
            const QImage img = videoFrame->asImage();
            const QImage transformed = spec.cpuApply ? spec.cpuApply(img, m_params) : img;
            if (!transformed.isNull())
                videoFrame = std::make_shared<VideoFrameData>(QVideoFrame(transformed));
        }
    }

    // ── Live display ────────────────────────────────────────────────────────
    // Present only while the live page is active. In editor-embedded mode the
    // static preview is shown and no frames are presented — zero GPU cost, and
    // the GL widget's context is never forced before its first show.
    VideoDisplayWidget *display = m_ui != nullptr ? m_ui->display() : nullptr;
    if (isLiveDisplayActive() && display != nullptr) {
        PERF_SCOPE("video", "output.present");
        if (videoFrame->isGpuRgba()) {
            VideoTextureHandle h;
            if (videoFrame->asTexture(&h))
                display->presentTexture(h, videoFrame);
            else
                display->presentImage(videoFrame->asImage());
        } else {
            VideoTextureHandle h;
            if (videoFrame->asTexture(&h))
                display->presentYuvTexture(h, videoFrame);
            else
                display->presentFrame(videoFrame->frame());
        }
    }

    // ── In-node preview ─────────────────────────────────────────────────────
    // Keep the frame and arm the single-shot timer, but ONLY while embedded in
    // the scene proxy — run/deembedded mode converts nothing.
    m_lastFrameForPreview = videoFrame;
    if (m_previewTimer != nullptr && m_ui != nullptr && m_ui->isEmbeddedInProxy()
        && !m_previewFired && !m_previewTimer->isActive())
        m_previewTimer->start();

    // Only emit when a downstream processing consumer is connected.
    if (m_outputConnectionCount > 0) {
        // Zero-copy passthrough (REQ-SW-PL-032): hand the SAME shared
        // VideoFrameData downstream — no readback, no re-wrap, no duplicate
        // upload. Residency (CPU / GpuYuv / GpuRgba) is preserved.
        m_output = videoFrame;
        Q_EMIT dataUpdated(0);
    }
}

void VideoOutputNode::outputConnectionCreated(QtNodes::ConnectionId const &conId)
{
    if (conId.outPortIndex == 0)
        ++m_outputConnectionCount;
}

void VideoOutputNode::outputConnectionDeleted(QtNodes::ConnectionId const &conId)
{
    if (conId.outPortIndex == 0 && m_outputConnectionCount > 0)
        --m_outputConnectionCount;
}

void VideoOutputNode::inputConnectionCreated(QtNodes::ConnectionId const &conId)
{
    if (conId.inPortIndex == 0)
        m_videoInputConnected = true;
}

void VideoOutputNode::inputConnectionDeleted(QtNodes::ConnectionId const &conId)
{
    if (conId.inPortIndex != 0)
        return;

    // The port-0 edge was removed. Disconnecting does NOT stop the source
    // player, so clear the flag that guards the port-0 branch FIRST, then reset
    // the video state.
    m_videoInputConnected = false;
    m_lastInput.reset();
    m_output.reset();
    resetVideoState();
}
