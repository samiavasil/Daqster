#include "test_audio_output_widget.h"

#include "Sinks/Audio/AudioOutputNode.h"
#include "Sinks/Audio/AudioOutputWidget.h"

#include "NodeDataTypes/SampledData.h"

#include <QtWidgets/QCheckBox>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QDoubleSpinBox>
#include <QtWidgets/QLabel>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QSpinBox>

#include <memory>

namespace {

/// Device id that never resolves — the widget and the node must handle a
/// config that points at hardware this machine does not have.
QString missingDeviceId()
{
    return QStringLiteral("no-such-output-device/REQ-SW-PL-076");
}

/// One 10 ms block of silence (48 frames @ 48 kHz, mono int16) — enough to make
/// the node arm itself, small enough that it never matters for the queue bound.
std::shared_ptr<SampledData> sampleBlock()
{
    SampledStreamDescriptor descriptor;
    descriptor.sampleRate = 48000.0;
    descriptor.domain = QStringLiteral("audio");
    StreamChannelDescriptor channel;
    channel.name = QStringLiteral("Mono");
    channel.sampleType = SampleType::INT16;
    descriptor.channels.append(channel);

    return std::make_shared<SampledData>(QByteArray(480 * 2, '\0'), descriptor);
}

AudioOutputNode::Config makeConfig()
{
    AudioOutputNode::Config config;
    config.deviceId = missingDeviceId();
    config.volume = 0.25;
    config.bufferMs = 500;
    config.enabled = true;
    return config;
}

} // namespace

void TestAudioOutputWidget::controlsShowModelConfig()
{
    AudioOutputNode node;
    node.setConfig(makeConfig());

    AudioOutputWidget widget(&node);

    auto* volume = widget.findChild<QDoubleSpinBox*>(QStringLiteral("volume"));
    auto* buffer = widget.findChild<QSpinBox*>(QStringLiteral("bufferMs"));
    auto* enabled = widget.findChild<QCheckBox*>(QStringLiteral("enabled"));
    auto* device = widget.findChild<QComboBox*>(QStringLiteral("deviceId"));

    QVERIFY(volume);
    QVERIFY(buffer);
    QVERIFY(enabled);
    QVERIFY(device);

    QCOMPARE(volume->value(), 0.25);
    QCOMPARE(buffer->value(), 500);
    QCOMPARE(enabled->isChecked(), true);

    // The unknown id is kept selected instead of falling back to the default,
    // so the user sees that the flow points at a device that is not here.
    QCOMPARE(device->currentData().toString(), missingDeviceId());
    QVERIFY(device->currentText().contains(missingDeviceId()));

    // Bounds mirror configSchema(): a widget with a wider range would accept a
    // value the model then clamps, and the two would disagree.
    QCOMPARE(volume->minimum(), 0.0);
    QCOMPARE(volume->maximum(), 1.0);
    QCOMPARE(buffer->minimum(), 20);
    QCOMPARE(buffer->maximum(), 2000);
}

void TestAudioOutputWidget::editingVolumeAndBufferReachesTheModel()
{
    AudioOutputNode node;
    node.setConfig(makeConfig());

    AudioOutputWidget widget(&node);
    auto* volume = widget.findChild<QDoubleSpinBox*>(QStringLiteral("volume"));
    auto* buffer = widget.findChild<QSpinBox*>(QStringLiteral("bufferMs"));

    volume->setValue(0.75);
    QCOMPARE(node.config().volume, 0.75);
    // …and it survives save(), which is what the .flow file keeps.
    QCOMPARE(node.save().value(QStringLiteral("volume")).toDouble(), 0.75);

    buffer->setValue(960);
    QCOMPARE(node.config().bufferMs, 960);
    QCOMPARE(node.save().value(QStringLiteral("bufferMs")).toInt(), 960);

    // Nothing else moved: the widget starts from the model's config, not from
    // a fresh one.
    QCOMPARE(node.config().deviceId, makeConfig().deviceId);
    QCOMPARE(node.config().enabled, true);
}

void TestAudioOutputWidget::enabledToggleReachesTheModelAndGreysTheControls()
{
    AudioOutputNode node;
    node.setConfig(makeConfig());

    AudioOutputWidget widget(&node);
    auto* enabled = widget.findChild<QCheckBox*>(QStringLiteral("enabled"));
    auto* volume = widget.findChild<QDoubleSpinBox*>(QStringLiteral("volume"));
    auto* device = widget.findChild<QComboBox*>(QStringLiteral("deviceId"));

    enabled->setChecked(false);
    QCOMPARE(node.config().enabled, false);

    // Tuning a sink that will not be opened is meaningless — greyed out, but
    // still readable (the value is what the file holds).
    QCOMPARE(volume->isEnabled(), false);
    QCOMPARE(device->isEnabled(), false);
    QCOMPARE(volume->value(), 0.25);

    enabled->setChecked(true);
    QCOMPARE(node.config().enabled, true);
    QCOMPARE(volume->isEnabled(), true);
    QCOMPARE(device->isEnabled(), true);
}

void TestAudioOutputWidget::loadRefillsTheControls()
{
    AudioOutputNode node;

    AudioOutputWidget widget(&node);
    auto* volume = widget.findChild<QDoubleSpinBox*>(QStringLiteral("volume"));
    auto* buffer = widget.findChild<QSpinBox*>(QStringLiteral("bufferMs"));
    auto* enabled = widget.findChild<QCheckBox*>(QStringLiteral("enabled"));
    auto* device = widget.findChild<QComboBox*>(QStringLiteral("deviceId"));

    QJsonObject json;
    json[QStringLiteral("deviceId")] = missingDeviceId();
    json[QStringLiteral("volume")] = 0.5;
    json[QStringLiteral("bufferMs")] = 320;
    json[QStringLiteral("enabled")] = false;

    // load() is what a `.flow` open and a REST POST /config do; the widget must
    // follow through configChanged, and must not write the values back (that
    // loop would trip the engine and re-create the sink on every load).
    node.load(json);

    QCOMPARE(volume->value(), 0.5);
    QCOMPARE(buffer->value(), 320);
    QCOMPARE(enabled->isChecked(), false);
    QCOMPARE(device->currentData().toString(), missingDeviceId());
    QCOMPARE(volume->isEnabled(), false);
    QCOMPARE(node.config().volume, 0.5);
}

void TestAudioOutputWidget::unknownPersistedDeviceStaysSelectable()
{
    AudioOutputNode node;
    AudioOutputNode::Config config;
    config.deviceId = missingDeviceId();
    node.setConfig(config);

    AudioOutputWidget widget(&node);
    auto* device = widget.findChild<QComboBox*>(QStringLiteral("deviceId"));

    // The pseudo-entry is present, selected, and carries the persisted id as
    // item data — so selecting it again (or saving without touching the combo)
    // keeps the id instead of rewriting it to "system default".
    QVERIFY(device->currentText().contains(QStringLiteral("unavailable")));
    QCOMPARE(device->currentData().toString(), config.deviceId);
    QCOMPARE(node.save().value(QStringLiteral("deviceId")).toString(), config.deviceId);
}

void TestAudioOutputWidget::statusLabelFollowsTheModel()
{
    AudioOutputNode node;
    node.setConfig(makeConfig());

    AudioOutputWidget widget(&node);
    auto* status = widget.findChild<QLabel*>(QStringLiteral("status"));
    QVERIFY(status);
    QCOMPARE(status->text(), QStringLiteral("stopped"));

    // start() with a device that does not exist: the label must explain the
    // no-op rather than stay on "stopped" while blocks arrive and vanish.
    node.start();
    QCOMPARE(status->text(), QStringLiteral("no output device"));
    QVERIFY(widget.findChild<QComboBox*>(QStringLiteral("deviceId"))->currentText().contains(QStringLiteral("unavailable")));
}

void TestAudioOutputWidget::startStopButtonArmsAndDisarmsTheEngine()
{
    AudioOutputNode node;
    node.setConfig(makeConfig());

    AudioOutputWidget widget(&node);
    auto* play = widget.findChild<QPushButton*>(QStringLiteral("playback"));
    QVERIFY(play);
    auto* enabled = widget.findChild<QCheckBox*>(QStringLiteral("enabled"));

    // Fresh node: stopped, and the button offers the only thing that helps.
    QVERIFY(!node.engine()->isRunning());
    QCOMPARE(play->text(), QStringLiteral("Start"));

    play->click();
    QVERIFY(node.engine()->isRunning());
    QCOMPARE(play->text(), QStringLiteral("Stop"));
    // The device id does not resolve here, so the status explains that instead
    // of pretending to play — the widget must show the model's text verbatim.
    QCOMPARE(widget.findChild<QLabel*>(QStringLiteral("status"))->text(),
             node.statusText());

    play->click();
    QVERIFY(!node.engine()->isRunning());
    QCOMPARE(play->text(), QStringLiteral("Start"));

    // Disabled: arming a node that skips every block would only mislead, so
    // the button greys out with the other controls.
    enabled->setChecked(false);
    QVERIFY(!play->isEnabled());
    enabled->setChecked(true);
    QVERIFY(play->isEnabled());
}

void TestAudioOutputWidget::autoArmFlipsTheButtonToStop()
{
    // The whole point of the auto-arm: the IDE calls no start(), so a block has
    // to arm playback. The button must then read "Stop" (the state), not "Start"
    // — otherwise the widget lies about a node that is already armed.
    AudioOutputNode node;
    node.setConfig(makeConfig());

    AudioOutputWidget widget(&node);
    auto* play = widget.findChild<QPushButton*>(QStringLiteral("playback"));
    QVERIFY(play);
    QCOMPARE(play->text(), QStringLiteral("Start"));

    node.setInData(sampleBlock(), 0);

    QVERIFY(node.engine()->isRunning());
    QCOMPARE(play->text(), QStringLiteral("Stop"));
    // This id never resolves, so the honest status is "no output device" —
    // armed and ready to play as soon as one exists.
    QCOMPARE(widget.findChild<QLabel*>(QStringLiteral("status"))->text(),
             node.statusText());
    QVERIFY(node.statusText().contains(QStringLiteral("no output device")));

    node.stop();
    QCOMPARE(play->text(), QStringLiteral("Start"));
}

QTEST_MAIN(TestAudioOutputWidget)
