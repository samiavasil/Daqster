#include "AudioOutputWidget.h"

#include "AudioCompat.h"
#include "Sinks/Audio/AudioOutputNode.h"

#include <QtCore/QSignalBlocker>
#include <QtWidgets/QCheckBox>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QDoubleSpinBox>
#include <QtWidgets/QFormLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QSpinBox>
#include <QtWidgets/QToolBox>
#include <QtWidgets/QVBoxLayout>

namespace {

// ── Bounds — the same as `AudioOutputNode::configSchema()` ───────────────
// Deliberately constants, not read from the JSON: the widget must not "fix" a
// value that came out of a `.flow` file and the two must not drift apart.
constexpr double kVolumeMin = 0.0;
constexpr double kVolumeMax = 1.0;
constexpr double kVolumeStep = 0.05;
constexpr int kVolumeDecimals = 2;

constexpr int kBufferMinMs = 20;
constexpr int kBufferMaxMs = 2000;
constexpr int kBufferStepMs = 20;

} // namespace

AudioOutputWidget::AudioOutputWidget(AudioOutputNode* model, QWidget* parent)
    : QWidget(parent)
    , m_model(model)
{
    Q_ASSERT(m_model);
    buildUi();
    reloadFromModel();
    setStatus(m_model->statusText());
}

void AudioOutputWidget::buildUi()
{
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);

    m_sections = new QToolBox(this);
    auto* page = new QWidget(m_sections);
    auto* form = new QFormLayout(page);
    form->setContentsMargins(6, 6, 6, 6);

    m_deviceCombo = new QComboBox(page);
    // Object names follow the persisted keys: they are how tests (and any
    // future schema-driven form) find the field a value belongs to.
    m_deviceCombo->setObjectName(QStringLiteral("deviceId"));
    m_deviceCombo->setMinimumWidth(180);
    // The list comes from the system, never from free text: a hand-typed id
    // resolves to no device and the node would report "no output device" on a
    // machine that has one.
    m_deviceCombo->setInsertPolicy(QComboBox::NoInsert);
    form->addRow(tr("Device"), m_deviceCombo);

    m_volumeBox = new QDoubleSpinBox(page);
    m_volumeBox->setObjectName(QStringLiteral("volume"));
    m_volumeBox->setRange(kVolumeMin, kVolumeMax);
    m_volumeBox->setDecimals(kVolumeDecimals);
    m_volumeBox->setSingleStep(kVolumeStep);
    m_volumeBox->setKeyboardTracking(false);   // no write per keystroke
    form->addRow(tr("Volume"), m_volumeBox);

    m_bufferBox = new QSpinBox(page);
    m_bufferBox->setObjectName(QStringLiteral("bufferMs"));
    m_bufferBox->setRange(kBufferMinMs, kBufferMaxMs);
    m_bufferBox->setSingleStep(kBufferStepMs);
    m_bufferBox->setSuffix(QStringLiteral(" ms"));
    m_bufferBox->setKeyboardTracking(false);
    m_bufferBox->setToolTip(tr("Device buffer in milliseconds. Not a limit on a "
                               "block: audio that does not fit is queued and fed "
                               "in chunks"));
    form->addRow(tr("Buffer"), m_bufferBox);

    m_enabledBox = new QCheckBox(tr("Enabled"), page);
    m_enabledBox->setObjectName(QStringLiteral("enabled"));
    m_enabledBox->setToolTip(tr("Off — no device is opened and incoming blocks "
                                "are skipped"));
    form->addRow(QString(), m_enabledBox);

    m_statusLabel = new QLabel(page);
    m_statusLabel->setObjectName(QStringLiteral("status"));
    m_statusLabel->setWordWrap(true);
    m_statusLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_statusLabel->setStyleSheet(QStringLiteral("color: gray;"));
    form->addRow(tr("Status"), m_statusLabel);

    m_sections->addItem(page, tr("Output"));
    root->addWidget(m_sections);

    // Every control writes through the model's setter — the widget keeps no
    // state of its own.
    connect(m_deviceCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int) { applyToModel(); });
    connect(m_volumeBox, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, [this](double) { applyToModel(); });
    connect(m_bufferBox, QOverload<int>::of(&QSpinBox::valueChanged),
            this, [this](int) { applyToModel(); });
    connect(m_enabledBox, &QCheckBox::toggled, this, [this](bool) {
        updateControlStates();
        applyToModel();
    });

    // The other direction: `load()` (a `.flow` opened, a REST config POST)
    // changed the model → the fields are re-filled.
    connect(m_model, &AudioOutputNode::configChanged,
            this, &AudioOutputWidget::reloadFromModel);
    connect(m_model, &AudioOutputNode::statusChanged,
            this, &AudioOutputWidget::setStatus);
}

void AudioOutputWidget::setDevices(const QStringList& descriptions,
                                   const QStringList& ids,
                                   int selectedIndex)
{
    const QSignalBlocker blocker(m_deviceCombo);
    m_deviceCombo->clear();
    m_deviceCombo->addItems(descriptions);
    for (int i = 0; i < ids.size() && i < m_deviceCombo->count(); ++i)
        m_deviceCombo->setItemData(i, ids.at(i));
    if (selectedIndex >= 0 && selectedIndex < m_deviceCombo->count())
        m_deviceCombo->setCurrentIndex(selectedIndex);
}

void AudioOutputWidget::reloadFromModel()
{
    // Each control is blocked separately: blocking `this` does not stop the
    // children's signals, and exactly those are wired to applyToModel().
    const QSignalBlocker blockDevices(m_deviceCombo);
    const QSignalBlocker blockVolume(m_volumeBox);
    const QSignalBlocker blockBuffer(m_bufferBox);
    const QSignalBlocker blockEnabled(m_enabledBox);

    const AudioOutputNode::Config config = m_model->config();

    QStringList descriptions;
    QStringList ids;
    descriptions << tr("System default");
    ids << QString();

    const QList<QAudioDeviceInfo> devices =
        AudioCompat::availableDevices(AudioCompat::AudioOutputMode);
    for (const QAudioDeviceInfo& device : devices) {
        descriptions << AudioCompat::deviceName(device);
        ids << AudioCompat::deviceId(device);
    }

    int selected = ids.indexOf(config.deviceId);
    if (selected < 0) {
        // The persisted device is gone (unplugged headset, another machine).
        // It is kept visible rather than silently replaced by the default: the
        // user has to see that the `.flow` points at a device this machine does
        // not have, and save() must not quietly rewrite it to "".
        descriptions << tr("%1 (unavailable)").arg(config.deviceId);
        ids << config.deviceId;
        selected = descriptions.size() - 1;
    }

    setDevices(descriptions, ids, selected);

    m_volumeBox->setValue(config.volume);
    m_bufferBox->setValue(config.bufferMs);
    m_enabledBox->setChecked(config.enabled);

    updateControlStates();
}

void AudioOutputWidget::setStatus(const QString& status)
{
    m_statusLabel->setText(status);
    // Presentation only: the engine already decided what happened, the label
    // just makes a failure stand out from "stopped"/"ready".
    const QString color = status.startsWith(QStringLiteral("error"))
                              ? QStringLiteral("red")
                              : (status.startsWith(QStringLiteral("playing"))
                                     ? QStringLiteral("green")
                                     : QStringLiteral("gray"));
    m_statusLabel->setStyleSheet(QStringLiteral("color: %1;").arg(color));
}

void AudioOutputWidget::applyToModel()
{
    // Start from the model's current config, not from a fresh one: a field
    // this widget does not show stays untouched.
    AudioOutputNode::Config config = m_model->config();

    config.deviceId = m_deviceCombo->currentData().toString();
    config.volume = m_volumeBox->value();
    config.bufferMs = m_bufferBox->value();
    config.enabled = m_enabledBox->isChecked();

    m_model->setConfig(config);
}

void AudioOutputWidget::updateControlStates()
{
    const bool active = m_enabledBox->isChecked();
    m_deviceCombo->setEnabled(active);
    m_volumeBox->setEnabled(active);
    m_bufferBox->setEnabled(active);
}
