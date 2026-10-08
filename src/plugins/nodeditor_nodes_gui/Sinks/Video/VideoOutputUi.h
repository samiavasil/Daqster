#pragma once

// SPDX-License-Identifier: MIT
//
// The widget side of the Video Output node (REQ-SW-PL-055 + REQ-SW-PL-051).
//
// Everything here is a QWidget and therefore belongs to nodeditor_nodes_gui.
// VideoOutputNode in _core owns the decisions — which backend, which frame, which
// effect, when the preview converts — and drives this object through IVideoOutputUi.
// This object reports user actions back through IVideoOutputUiHandler.
//
// Layout (ported from consolidate, which had no core/gui split and could build
// all of it inside the node model):
//
//   rootWidget  (QWidget)
//   └─ m_videoStack (QStackedWidget)
//      ├─ page 0  m_previewPage — static QLabel snapshot (editor canvas only)
//      └─ page 1  m_livePage    — QSplitter
//                    ├─ left   display()->widget()   (GL blit or software)
//                    └─ right  m_controlsWidget     (perf toggle + stats + effects)

#include <QObject>
#include <QVector>

#include "IVideoOutputUi.h"
#include "VideoEffectOps.h"

class QCheckBox;
class QComboBox;
class QEvent;
class QLabel;
class QSplitter;
class QStackedWidget;
class QTimer;
class QWidget;
class VideoDisplayWidget;

// QObject because this really is a widget object: it needs the event filter, the
// connect() sugar and tr(). The INTERFACE stays a plain non-QObject type, so
// _core keeps holding it without moc and without a meta-object registration.
class VideoOutputUi : public QObject, public IVideoOutputUi
{
    Q_OBJECT

public:
    /// params is NOT owned here — it is the EffectParams instance inside
    /// VideoOutputNode. The parameter widgets write into it directly, which is
    /// why onEffectParamsChanged() needs no arguments: by the time _core hears
    /// about the change, the new values are already in place.
    VideoOutputUi(const QVector<EffectSpec> &specs, EffectParams *params,
                  IVideoOutputUiHandler *handler, QWidget *parent = nullptr);
    ~VideoOutputUi() override;

    // ── IVideoOutputUi ────────────────────────────────────────────────────
    QWidget *rootWidget() const override { return m_root; }
    VideoDisplayWidget *display() const override { return m_display; }
    QWidget *controlsWidget() const override { return m_controlsWidget; }
    void setLiveActive(bool live) override;
    bool isLiveActive() const override;
    bool isEmbeddedInProxy() const override;
    void setPreviewImage(const QImage &image) override;
    void clearPreview() override;
    bool previewVisible() const override;
    void setEffectPage(int comboIndex) override;
    void syncEffectParams() override;
    void updatePerfStats(const VideoPerfStats &stats) override;
    void setPerfChecked(bool checked) override;
    bool perfChecked() const override;

    // ── IVideoOutputUiHandler sinks ───────────────────────────────────────
    /// Root widget Show/Hide. A QOpenGLWidget cannot live in a
    /// QGraphicsProxyWidget, so the live/preview decision follows proxy state;
    /// reporting it is all this side can do, because only _core owns the preview
    /// timer and its once-per-Play latch.
    bool eventFilter(QObject *watched, QEvent *event) override;

    /// Timer that drives the display's repaint is not needed here — the backends
    /// repaint themselves. Exposed only so the test can force a paint.
    QWidget *displayWidget() const;

private:
    QWidget *createPreviewPage();
    QWidget *createControlsWidget();
    QWidget *createEffectPage(int specIndex);
    QWidget *createSliderPage(int &value, int min, int max, const QString &title);
    QWidget *createFlipPage();
    QWidget *createCannyPage();
    QWidget *createInfoPage(const QString &text);

    void setPlaceholder(const QString &text);
    void refreshPerfLabels(const VideoPerfStats &stats);

    QWidget *m_root = nullptr;
    QStackedWidget *m_videoStack = nullptr;
    QWidget *m_previewPage = nullptr;
    QWidget *m_livePage = nullptr;
    QLabel *m_preview = nullptr;

    QSplitter *m_splitter = nullptr;
    QWidget *m_controlsWidget = nullptr;
    VideoDisplayWidget *m_display = nullptr;

    QCheckBox *m_perfToggle = nullptr;
    QLabel *m_fpsLabel = nullptr;
    QLabel *m_gapLabel = nullptr;
    QLabel *m_presentLabel = nullptr;
    QLabel *m_totalLabel = nullptr;
    QLabel *m_cpuLabel = nullptr;
    QLabel *m_hwSwLabel = nullptr;
    QLabel *m_formatLabel = nullptr;
    QLabel *m_handleLabel = nullptr;

    QComboBox *m_effectCombo = nullptr;
    QStackedWidget *m_effectStack = nullptr;

    QVector<EffectSpec> m_specs;
    EffectParams *m_params = nullptr;
    IVideoOutputUiHandler *m_handler = nullptr;
};
