#include "AudioSinkWidget.h"

#include "NodeWidgetDialogs.h"
#include "Sinks/Audio/AudioSinkNode.h"

#include <QtCore/QSignalBlocker>
#include <QtWidgets/QCheckBox>
#include <QtWidgets/QDoubleSpinBox>
#include <QtWidgets/QFileDialog>
#include <QtWidgets/QFormLayout>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QToolBox>
#include <QtWidgets/QVBoxLayout>

namespace {

// ── Bounds — the same as `AudioSinkNode::configSchema()` ─────────────────
// Deliberately constants, not read from the JSON: the widget must not "fix" a
// value that came out of a `.flow` file and the two must not drift apart.
constexpr double kVolumeMin = 0.0;
constexpr double kVolumeMax = 1.0;
constexpr double kVolumeStep = 0.01;
constexpr int kVolumeDecimals = 2;

/// Minimum width: the path to the WAV is a long string.
constexpr int kMinWidth = 340;

} // namespace

AudioSinkWidget::AudioSinkWidget(AudioSinkNode* model, QWidget* parent)
    : QWidget(parent)
    , m_model(model)
{
    Q_ASSERT(m_model);
    buildUi();
    reloadFromModel();
}

void AudioSinkWidget::buildUi()
{
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);

    auto* sections = new QToolBox(this);
    auto* page = new QWidget(sections);
    auto* form = new QFormLayout(page);
    form->setContentsMargins(6, 6, 6, 6);

    // Object names follow the persisted keys: they are how tests (and any
    // future schema-driven form) find the field a value belongs to.
    m_outputPath = new QLineEdit(page);
    m_outputPath->setObjectName(QStringLiteral("outputPath"));
    m_outputPath->setPlaceholderText(tr("No file — nothing is written"));

    auto* browse = new QPushButton(QStringLiteral("\u2026"), page);
    browse->setObjectName(QStringLiteral("browse"));
    browse->setToolTip(tr("Choose the WAV file to write"));
    // The dialog opens only on click — not while the widget is being built
    // (REQ-SW-PL-059: an embedded widget's dialog parent is offscreen).
    connect(browse, &QPushButton::clicked, this, &AudioSinkWidget::browseOutputPath);

    auto* pathRow = new QWidget(page);
    auto* pathLayout = new QHBoxLayout(pathRow);
    pathLayout->setContentsMargins(0, 0, 0, 0);
    pathLayout->setSpacing(4);
    pathLayout->addWidget(m_outputPath, 1);
    pathLayout->addWidget(browse);
    form->addRow(tr("WAV file"), pathRow);

    m_playbackEnabled = new QCheckBox(tr("Play back"), page);
    m_playbackEnabled->setObjectName(QStringLiteral("playbackEnabled"));
    m_playbackEnabled->setToolTip(tr("Deprecated — use the AudioOutput node. Kept "
                                     "so an older .flow still plays its audio"));
    form->addRow(QString(), m_playbackEnabled);

    m_volume = new QDoubleSpinBox(page);
    m_volume->setObjectName(QStringLiteral("volume"));
    m_volume->setRange(kVolumeMin, kVolumeMax);
    m_volume->setDecimals(kVolumeDecimals);
    m_volume->setSingleStep(kVolumeStep);
    m_volume->setKeyboardTracking(false);   // no write per keystroke
    m_volume->setToolTip(tr("Playback volume, 0..1"));
    form->addRow(tr("Volume"), m_volume);

    sections->addItem(page, tr("Output"));
    root->addWidget(sections);

    // Every control writes through the model's setter — the widget keeps no
    // state of its own. `textEdited` (not `textChanged`) so a block from
    // reloadFromModel() cannot loop back into the model.
    connect(m_outputPath, &QLineEdit::textEdited,
            this, [this](const QString&) { applyToModel(); });
    connect(m_playbackEnabled, &QCheckBox::toggled,
            this, [this](bool) { applyToModel(); });
    connect(m_volume, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, [this](double) { applyToModel(); });

    // The other direction: `load()` (a `.flow` opened, a REST config POST)
    // changed the model → the fields are re-filled.
    connect(m_model, &AudioSinkNode::configChanged,
            this, &AudioSinkWidget::reloadFromModel);
}

void AudioSinkWidget::reloadFromModel()
{
    // Each control is blocked separately: blocking `this` does not stop the
    // children's signals, and exactly those are wired to applyToModel().
    const QSignalBlocker blockPath(m_outputPath);
    const QSignalBlocker blockPlayback(m_playbackEnabled);
    const QSignalBlocker blockVolume(m_volume);

    m_outputPath->setText(m_model->outputPath());
    m_playbackEnabled->setChecked(m_model->playbackEnabled());
    m_volume->setValue(m_model->volume());
}

void AudioSinkWidget::applyToModel()
{
    // Start from the model's current config, not from a fresh one: a field
    // this widget does not show stays untouched.
    AudioSinkNode::Config config = m_model->config();

    config.outputPath = m_outputPath->text().trimmed();
    config.playbackEnabled = m_playbackEnabled->isChecked();
    config.volume = m_volume->value();

    m_model->setConfig(config);
}

void AudioSinkWidget::browseOutputPath()
{
    const QString chosen = QFileDialog::getSaveFileName(
        Daqster::nodeWidgetDialogParent(this), tr("WAV output"), m_outputPath->text(),
        tr("WAV files (*.wav);;All files (*)"));

    if (chosen.isEmpty())
        return;   // cancel changes nothing

    // `setText()` does not emit `textEdited`, so the model is updated here —
    // the same single write path, just not driven by a keystroke.
    m_outputPath->setText(chosen);
    applyToModel();
}

QSize AudioSinkWidget::sizeHint() const
{
    const QSize base = QWidget::sizeHint();
    return QSize(qMax(base.width(), kMinWidth), base.height());
}
