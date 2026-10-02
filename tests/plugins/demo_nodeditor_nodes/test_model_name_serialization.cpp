#include "test_model_name_serialization.h"

#include <Displays/NumberDisplay/NumberDisplayDataModel.h>
#include <Operators/ArithmeticLogic/ArithmeticLogicModel.h>
#include <Operators/ModuloModel.h>
#include <Sources/AudioSource/AudioSourceDataModel.h>
#include <Sources/GpuMonitor/GpuMonitorModel.h>
#include <Sources/JackDetect/JackDetectModel.h>
#include <Sources/Pcap/PcapModel.h>

// The invariant every save() must satisfy.
//
// The graph serializer instantiates a node from
// `<node>["internal-data"]["model-name"]` (DataFlowGraphModel::loadNode).
// An empty string there makes registry->create() return nullptr, so the node is
// dropped during load and reported as "<unnamed>" — together with every
// connection that referenced it. Nothing warns at save time, which is what
// makes this worth asserting directly.
namespace {

void assertCarriesModelName(const QtNodes::NodeDelegateModel& model)
{
    const QJsonObject json = model.save();

    QVERIFY2(json.contains(QStringLiteral("model-name")),
             qPrintable(QStringLiteral("%1: save() dropped \"model-name\" — the node "
                                       "cannot be restored from a .flow file")
                            .arg(QString::fromLatin1(model.name().toUtf8().constData()))));
    QCOMPARE(json.value(QStringLiteral("model-name")).toString(), model.name());
}

} // namespace

void ModelNameSerializationTest::audioSource_savesModelName()
{
    AudioSourceDataModel node;
    assertCarriesModelName(node);
}

void ModelNameSerializationTest::gpuMonitor_savesModelName()
{
    GpuMonitorModel node;
    assertCarriesModelName(node);
}

void ModelNameSerializationTest::jackDetect_savesModelName()
{
    JackDetectModel node;
    assertCarriesModelName(node);
}

void ModelNameSerializationTest::pcap_savesModelName()
{
    PcapModel node;
    assertCarriesModelName(node);
}

void ModelNameSerializationTest::numberDisplay_savesModelName()
{
    NumberDisplayDataModel node;
    assertCarriesModelName(node);
}

void ModelNameSerializationTest::arithmeticLogic_savesModelName()
{
    ArithmeticLogicModel node;
    assertCarriesModelName(node);
}

void ModelNameSerializationTest::modulo_savesModelName()
{
    ModuloModel node;
    assertCarriesModelName(node);
}

// The bare "name" key was written by the same broken save() implementations and
// read by nobody — the type lives in "model-name". Leaving it in place invites
// the confusion that produced the defect, so it is asserted gone.
void ModelNameSerializationTest::save_doesNotWriteBareNameKey()
{
    AudioSourceDataModel audio;
    QVERIFY(!audio.save().contains(QStringLiteral("name")));

    PcapModel pcap;
    QVERIFY(!pcap.save().contains(QStringLiteral("name")));

    NumberDisplayDataModel display;
    QVERIFY(!display.save().contains(QStringLiteral("name")));
}

// Starting from the base object's result must ADD "model-name" without
// displacing the state keys the models' own load() implementations read.
void ModelNameSerializationTest::modelSpecificKeysSurvive()
{
    NumberDisplayDataModel display;
    const QJsonObject displayJson = display.save();
    QCOMPARE(displayJson.value(QStringLiteral("model-name")).toString(),
             display.name());
    QVERIFY2(displayJson.contains(QStringLiteral("type")),
             "NumberDisplayDataModel::load() reads \"type\" — save() must keep it");

    GpuMonitorModel gpu;
    const QJsonObject gpuJson = gpu.save();
    QCOMPARE(gpuJson.value(QStringLiteral("model-name")).toString(), gpu.name());
    QVERIFY2(gpuJson.contains(QStringLiteral("intervalSeconds")),
             "GpuMonitorModel::load() reads \"intervalSeconds\" — save() must keep it");

    JackDetectModel jack;
    const QJsonObject jackJson = jack.save();
    QCOMPARE(jackJson.value(QStringLiteral("model-name")).toString(), jack.name());
    QVERIFY2(jackJson.contains(QStringLiteral("intervalSeconds")),
             "JackDetectModel::load() reads \"intervalSeconds\" — save() must keep it");

    ArithmeticLogicModel arithmetic;
    const QJsonObject arithmeticJson = arithmetic.save();
    QCOMPARE(arithmeticJson.value(QStringLiteral("model-name")).toString(),
             arithmetic.name());
    QVERIFY2(arithmeticJson.contains(QStringLiteral("inputs")),
             "ArithmeticLogicModel::load() reads \"inputs\" — save() must keep it");
}

QTEST_MAIN(ModelNameSerializationTest)
