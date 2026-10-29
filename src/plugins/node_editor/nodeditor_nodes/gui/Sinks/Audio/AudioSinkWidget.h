#ifndef AUDIOSINKWIDGET_H
#define AUDIOSINKWIDGET_H

#include <QSize>
#include <QWidget>

class QCheckBox;
class QDoubleSpinBox;
class QLineEdit;

class AudioSinkNode;

/**
 * @brief GUI widget of the Audio sink node (REQ-SW-PL-077).
 *
 * Shows the three persisted fields of `AudioSinkNode::Config` — WAV path,
 * playback (deprecated), volume — and nothing else: the widget is a view, not a
 * second copy of the state. Every change goes through
 * `AudioSinkNode::setConfig()`, and `reloadFromModel()` re-reads the model when
 * `load()` (a `.flow` opened, a REST config POST) changed it.
 *
 * `outputPath` is shown **as it is** in the model (empty = no recording). The
 * file dialog opens **only** when the "…" button is pressed — never while the
 * widget is being built and never from `configChanged()`, because the widget is
 * also constructed inside the editor's `QGraphicsProxyWidget`, where a modal
 * dialog would be invisible yet modal (REQ-SW-PL-059: the parent comes from
 * `Daqster::nodeWidgetDialogParent()`).
 *
 * The playback checkbox is kept because `.flow` files carry the key, but it is
 * marked deprecated: a new graph wires an `AudioOutput` node instead.
 */
class AudioSinkWidget : public QWidget
{
    Q_OBJECT

public:
    /**
     * @param model The node model this widget edits (not owned).
     * @param parent Parent widget (ownership of the memory).
     */
    explicit AudioSinkWidget(AudioSinkNode* model, QWidget* parent = nullptr);
    ~AudioSinkWidget() override = default;

    /// The path field does not shrink below a usable width — a WAV path is long.
    QSize sizeHint() const override;

public slots:
    /// Model → widget: re-read every field from `AudioSinkNode::config()`.
    void reloadFromModel();

private slots:
    /// Ask for a WAV path and put it in the field (and from there in the model).
    void browseOutputPath();

private:
    void buildUi();

    /// Widget → model: one `Config`, started from the model's current one so a
    /// field this widget does not show is never reset to a default.
    void applyToModel();

    AudioSinkNode* m_model = nullptr;

    QLineEdit* m_outputPath = nullptr;
    QCheckBox* m_playbackEnabled = nullptr;
    QDoubleSpinBox* m_volume = nullptr;
};

#endif // AUDIOSINKWIDGET_H
