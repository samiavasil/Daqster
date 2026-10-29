#include "test_audio_sink_widget.h"

#include "Sinks/Audio/AudioSinkNode.h"
#include "Sinks/Audio/AudioSinkWidget.h"

#include <QSignalSpy>
#include <QtWidgets/QCheckBox>
#include <QtWidgets/QDoubleSpinBox>
#include <QtWidgets/QLineEdit>

namespace {

AudioSinkNode::Config makeConfig()
{
    AudioSinkNode::Config config;
    config.outputPath = QStringLiteral("/tmp/daqster-audio-sink-widget.wav");
    config.playbackEnabled = false;   // never open a device in a test
    config.volume = 0.30;
    return config;
}

} // namespace

void TestAudioSinkWidget::controlsShowModelConfig()
{
    AudioSinkNode node;
    node.setConfig(makeConfig());

    AudioSinkWidget widget(&node);

    auto* path = widget.findChild<QLineEdit*>(QStringLiteral("outputPath"));
    auto* playback = widget.findChild<QCheckBox*>(QStringLiteral("playbackEnabled"));
    auto* volume = widget.findChild<QDoubleSpinBox*>(QStringLiteral("volume"));

    QVERIFY(path);
    QVERIFY(playback);
    QVERIFY(volume);

    QCOMPARE(path->text(), makeConfig().outputPath);
    QCOMPARE(playback->isChecked(), false);
    QCOMPARE(volume->value(), 0.30);
}

void TestAudioSinkWidget::editingVolumeReachesTheModel()
{
    AudioSinkNode node;
    node.setConfig(makeConfig());

    AudioSinkWidget widget(&node);
    auto* volume = widget.findChild<QDoubleSpinBox*>(QStringLiteral("volume"));
    QVERIFY(volume);

    volume->setValue(0.55);

    QCOMPARE(node.config().volume, 0.55);
    // …and it survives save(), which is what the .flow file keeps.
    QCOMPARE(node.save().value(QStringLiteral("volume")).toDouble(), 0.55);

    // The fields this widget did not touch are untouched: applyToModel() starts
    // from the model's config, not from a fresh one.
    QCOMPARE(node.config().outputPath, makeConfig().outputPath);
    QCOMPARE(node.config().playbackEnabled, false);
}

void TestAudioSinkWidget::editingPlaybackFlagReachesTheModel()
{
    AudioSinkNode node;
    node.setConfig(makeConfig());

    AudioSinkWidget widget(&node);
    auto* playback = widget.findChild<QCheckBox*>(QStringLiteral("playbackEnabled"));
    QVERIFY(playback);

    // The key is kept for .flow compatibility, so the checkbox still writes it —
    // the model delegates the actual playback to the persistent engine, and
    // nothing is opened until a block arrives.
    playback->setChecked(true);
    QCOMPARE(node.config().playbackEnabled, true);
    QCOMPARE(node.save().value(QStringLiteral("playbackEnabled")).toBool(), true);

    playback->setChecked(false);
    QCOMPARE(node.config().playbackEnabled, false);
    QCOMPARE(node.config().outputPath, makeConfig().outputPath);
}

void TestAudioSinkWidget::loadRefillsTheControlsWithoutWritingBack()
{
    AudioSinkNode node;

    AudioSinkWidget widget(&node);
    auto* path = widget.findChild<QLineEdit*>(QStringLiteral("outputPath"));
    auto* playback = widget.findChild<QCheckBox*>(QStringLiteral("playbackEnabled"));
    auto* volume = widget.findChild<QDoubleSpinBox*>(QStringLiteral("volume"));
    QVERIFY(path && playback && volume);

    QSignalSpy configSpy(&node, &AudioSinkNode::configChanged);

    QJsonObject json;
    json[QStringLiteral("model-name")] = QStringLiteral("AudioSinkNode");
    json[QStringLiteral("outputPath")] = QStringLiteral("/tmp/from-flow.wav");
    json[QStringLiteral("playbackEnabled")] = false;
    json[QStringLiteral("volume")] = 0.75;

    // load() is what a .flow open and a REST POST /config do.
    node.load(json);

    QCOMPARE(path->text(), QStringLiteral("/tmp/from-flow.wav"));
    QCOMPARE(playback->isChecked(), false);
    QCOMPARE(volume->value(), 0.75);

    // Exactly one setConfig() — the load's own. A write-back loop would emit a
    // second one, and in a live graph that re-opens the recording (and the
    // device when playback is enabled) on every load.
    QCOMPARE(configSpy.count(), 1);
    QCOMPARE(node.config().outputPath, QStringLiteral("/tmp/from-flow.wav"));
}

void TestAudioSinkWidget::schemaBoundsMatchTheControls()
{
    AudioSinkNode node;
    AudioSinkWidget widget(&node);

    auto* volume = widget.findChild<QDoubleSpinBox*>(QStringLiteral("volume"));
    QVERIFY(volume);

    // Bounds mirror configSchema(): a widget with a wider range would accept a
    // value the model then clamps, and the two would disagree.
    const QJsonObject property = node.configSchema()
                                     .value(QStringLiteral("properties")).toObject()
                                     .value(QStringLiteral("volume")).toObject();
    QCOMPARE(volume->minimum(), property.value(QStringLiteral("minimum")).toDouble());
    QCOMPARE(volume->maximum(), property.value(QStringLiteral("maximum")).toDouble());
}

void TestAudioSinkWidget::playbackFlagIsMarkedDeprecated()
{
    AudioSinkNode node;
    AudioSinkWidget widget(&node);

    auto* playback = widget.findChild<QCheckBox*>(QStringLiteral("playbackEnabled"));
    QVERIFY(playback);

    // The public audio path is the separate AudioOutput node; the checkbox only
    // exists so an older .flow keeps working, and it has to say so.
    QVERIFY(playback->toolTip().contains(QStringLiteral("Deprecated")));
    QVERIFY(playback->toolTip().contains(QStringLiteral("AudioOutput")));
}

QTEST_MAIN(TestAudioSinkWidget)
