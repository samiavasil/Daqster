#pragma once

// SPDX-License-Identifier: MIT
//
// The core/gui seam for the Video Output node's UI (REQ-SW-PL-055 + REQ-SW-PL-051).
//
// VideoOutputNode lives in nodeditor_nodes/core and pushes frames through a
// VideoDisplayWidget every frame. The concrete display backends, the splitter,
// the preview label, the perf panel and the effect parameter widgets are all
// QWidget and therefore live in nodeditor_nodes/gui. _core cannot include
// or link them — that was the last hard-coded widget dependency in this plugin.
//
// So the two sides talk through the two plain interfaces below:
//
//   core -> gui   IVideoOutputUi         core holds the pointer, calls it
//   gui  -> core  IVideoOutputUiHandler  gui holds the pointer, calls it
//
// Both are non-QObject, header-only, pure virtual. That matters for two reasons:
// a plain interface needs no moc and no meta-object registration, and it can be
// held across a plugin boundary without any link dependency — the shared type is
// in _core's own headers, and the concrete object is in _gui.
//
// This is deliberately NOT the direction used for the editor calling into the
// node (setPerfEnabled / forceLiveDisplay). Those callers live in a THIRD plugin
// that must not depend on _core or _gui at link time, so those two stay
// Q_INVOKABLE and are reached through QMetaObject::invokeMethod. Two mechanisms
// for two genuinely different constraints, not an inconsistency.

#include <QImage>
#include <QString>

#include "VideoEffectOps.h"

class QWidget;
class VideoDisplayWidget;

/// One snapshot of the "video" perf domain, sampled by _core on its own 500 ms
/// timer and rendered into labels by _gui. Sampling stays in core because the
/// Perf::Domain and ProcessCpu are core facilities; only the text is a widget.
struct VideoPerfStats
{
    double fps = 0.0;
    double gapMs = 0.0;
    double presentMs = 0.0;
    double totalMs = 0.0;
    double cpuPercent = 0.0;
    /// From the display backend: true = GL blit, false = software.
    bool hardwareGL = false;
    /// QVideoFrame::handleType() of the last presented frame.
    int handleType = 0;
    /// Normalized pixel format of the last presented frame (Qt6 numbering, see
    /// VideoCompat::pixelFormatInt) so both Qt versions display the same name.
    int pixelFormat = -1;
};

/// _core calls this on every frame and on every mode change.
class IVideoOutputUi
{
public:
    virtual ~IVideoOutputUi() = default;

    /// The node widget as a whole — the thing the node editor embeds. _core uses
    /// it for graphicsProxyWidget(), which is what decides preview vs live page:
    /// a QOpenGLWidget cannot be hosted inside a QGraphicsProxyWidget (Qt paints
    /// WA_PaintOnScreen widgets on their own layer), so the GL display may only
    /// live on the page shown when there is no proxy.
    virtual QWidget *rootWidget() const = 0;

    /// The unified display backend (VideoGLBlitWidget or VideoSoftwareWidget).
    /// Never null. _core presents every frame through this pointer.
    virtual VideoDisplayWidget *display() const = 0;

    /// The controls widget (perf toggle + perf stats + effect parameters), the
    /// right pane of the live splitter. Diagnostic accessor — _core never lays it
    /// out, the UI already did.
    virtual QWidget *controlsWidget() const = 0;

    /// Switch between the static preview page (inProxy = true, editor canvas)
    /// and the live page (inProxy = false, deembedded window or run mode).
    virtual void setLiveActive(bool live) = 0;
    virtual bool isLiveActive() const = 0;

    /// True when the root widget is currently inside a QGraphicsProxyWidget.
    virtual bool isEmbeddedInProxy() const = 0;

    /// Set the static preview snapshot (already scaled — _core does the
    /// scale-before-convert so the full frame is never materialized).
    virtual void setPreviewImage(const QImage &image) = 0;

    /// Restore the "No video" placeholder on the preview page.
    virtual void clearPreview() = 0;

    /// Whether the preview label is visible. _core's single-shot preview timer
    /// checks this and skips the conversion entirely when the canvas is hidden
    /// (standalone --run mode), which is what makes that mode cost zero.
    virtual bool previewVisible() const = 0;

    /// Show the effect parameter page for a combo index (0 = "No effect").
    virtual void setEffectPage(int comboIndex) = 0;

    /// Rebuild the parameter widgets so they show the values now sitting in the
    /// EffectParams instance the UI was given at construction. Called from
    /// VideoOutputNode::load() — the struct is shared by pointer, so there is
    /// nothing to pass: the widgets read it, and that is the source of truth.
    virtual void syncEffectParams() = 0;

    /// Refresh the perf panel labels.
    virtual void updatePerfStats(const VideoPerfStats &stats) = 0;

    /// Persisted perf toggle state (save/load). Setting it emits toggled, which
    /// is what enables the "video" domain and starts the timers.
    virtual void setPerfChecked(bool checked) = 0;
    virtual bool perfChecked() const = 0;
};

/// _gui calls this when the user touches a control. Implemented by
/// VideoOutputNode in _core.
class IVideoOutputUiHandler
{
public:
    virtual ~IVideoOutputUiHandler() = default;

    /// The root widget became visible or hidden, or its proxy state changed.
    /// _gui cannot decide what this means (only core owns the preview timer and
    /// its latch), so it just reports the observed state.
    virtual void onUiVisibilityChanged(bool inProxy) = 0;

    /// The effect combo changed. comboIndex 0 is the "No effect" placeholder.
    virtual void onEffectSelectionChanged(int comboIndex) = 0;

    /// A parameter widget changed. The widgets already wrote into the
    /// EffectParams instance owned by the model, so this only has to ask for a
    /// reprocess of the current frame.
    virtual void onEffectParamsChanged() = 0;

    /// The perf checkbox changed. Toggles the "video" domain and the timers.
    virtual void onPerfToggled(bool enabled) = 0;
};
