#include "test_node_config_schemas.h"

// The canonical interface header, the same one the control plane includes
// (framework_core/base/src/engine/HeadlessEngine.cpp) — asserting the cast
// through a different header than the production caller would assert a
// different cast.
#include <capabilities/INodeConfigSchema.h>

// The models under test. Every constructor below is inert without its device
// (they only *enumerate* what they would open), which is why this suite needs
// no hardware.
#include <Sinks/FileRecord/FileRecordModel.h>
#include <Sinks/NetworkSink/NetworkSinkModel.h>
#include <Sources/FilePlayback/FilePlaybackModel.h>
#include <Sources/Gamepad/GamepadModel.h>
#include <Sources/GpuMonitor/GpuMonitorModel.h>
#include <Sources/JackDetect/JackDetectModel.h>
#include <Sources/NetworkSource/NetworkSourceModel.h>
#include <Sources/SystemMonitor/SystemMonitorModel.h>
#include <Sources/Video/CameraSourceNode.h>
#include <Sources/Video/StreamSourceNode.h>
#include <Sources/Video/VideoFileSourceNode.h>

#ifdef HAVE_LIBIIO
// PlutoSdrEngine calls iio_* directly, so the model only exists in this target
// when the SDK was found at configure time (same guard as test_thread_lifecycle).
#include <Sources/PlutoSdr/PlutoSdrModel.h>
#endif

#include <QtNodes/NodeDelegateModel>

#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QSet>
#include <QStringList>

namespace {

/// Whether a `save()` value of type @p value fits the schema's `"type"` word.
bool typeFits(const QJsonValue& value, const QString& schemaType)
{
    switch (value.type()) {
    case QJsonValue::Bool:
        return schemaType == QLatin1String("boolean");
    case QJsonValue::Double:
        // A JSON number is the only numeric type there is; "integer" is the
        // schema's narrowing of it, and is what a port count / rate deserves.
        return schemaType == QLatin1String("number") || schemaType == QLatin1String("integer");
    case QJsonValue::String:
        return schemaType == QLatin1String("string");
    case QJsonValue::Array:
        return schemaType == QLatin1String("array");
    case QJsonValue::Object:
        return schemaType == QLatin1String("object");
    default:
        return schemaType == QLatin1String("null");
    }
}

} // namespace

void TestNodeConfigSchemas::verifySchemaMatchesSave(QtNodes::NodeDelegateModel& model,
                                                    const QStringList& conditionalKeys)
{
    const QString type = model.name();
    auto* described = dynamic_cast<const Daqster::INodeConfigSchema*>(&model);
    QVERIFY2(described != nullptr,
             qPrintable(type + QStringLiteral(" must implement INodeConfigSchema: without it the "
                                              "control plane answers restartRequired from a "
                                              "derived schema")));

    const QJsonObject schema = described->configSchema();
    QCOMPARE(schema.value(QStringLiteral("type")).toString(), QStringLiteral("object"));
    const QJsonObject properties = schema.value(QStringLiteral("properties")).toObject();
    QVERIFY2(!properties.isEmpty(), qPrintable(type + QStringLiteral(": empty schema")));

    const QJsonObject saved = model.save();
    const QSet<QString> conditional(conditionalKeys.begin(), conditionalKeys.end());

    // Direction 1: every key save() emits (but the base's read-only model-name)
    // is described and marked — a field the control plane cannot classify.
    for (auto it = saved.constBegin(); it != saved.constEnd(); ++it) {
        if (it.key() == QLatin1String("model-name"))
            continue;
        const QString key = it.key();
        QVERIFY2(properties.contains(key),
                 qPrintable(type + QStringLiteral(": save() emits '%1' that configSchema() does "
                                                  "not describe").arg(key)));
        const QJsonObject property = properties.value(key).toObject();
        QVERIFY2(property.value(QStringLiteral("x-restart")).isBool(),
                 qPrintable(type + QStringLiteral(": '%1' carries no boolean \"x-restart\"").arg(key)));
        QVERIFY2(typeFits(it.value(), property.value(QStringLiteral("type")).toString()),
                 qPrintable(type + QStringLiteral(": '%1' is saved as a different JSON type than "
                                                  "the schema declares").arg(key)));
        QVERIFY2(!conditional.contains(key),
                 qPrintable(type + QStringLiteral(": '%1' is listed as conditional but save() "
                                                  "emits it").arg(key)));
    }

    // Direction 2: every described property is a key save() really has, so a
    // renamed or removed field cannot leave a stale marker behind.
    for (auto it = properties.constBegin(); it != properties.constEnd(); ++it) {
        const QString key = it.key();
        if (key == QLatin1String("model-name"))
            continue;
        QVERIFY2(saved.contains(key) || conditional.contains(key),
                 qPrintable(type + QStringLiteral(": configSchema() describes '%1' that save() "
                                                  "never emits").arg(key)));
    }

    // Nothing beyond those two directions: `additionalProperties: false` would
    // otherwise let the schema claim a closed object it does not describe.
    QCOMPARE(schema.value(QStringLiteral("additionalProperties")).toBool(true), false);
}

void TestNodeConfigSchemas::cameraSource_schemaCoversSaveKeys()
{
    CameraSourceNode model;
    // save() omits cameraId while no camera is attached (headless CI has none).
    verifySchemaMatchesSave(model, {QStringLiteral("cameraId")});
}

void TestNodeConfigSchemas::videoFileSource_schemaCoversSaveKeys()
{
    VideoFileSourceNode model;
    verifySchemaMatchesSave(model);
}

void TestNodeConfigSchemas::streamSource_schemaCoversSaveKeys()
{
    StreamSourceNode model;
    verifySchemaMatchesSave(model);
}

void TestNodeConfigSchemas::filePlayback_schemaCoversSaveKeys()
{
    FilePlaybackModel model;
    verifySchemaMatchesSave(model);
}

void TestNodeConfigSchemas::fileRecord_schemaCoversSaveKeys()
{
    FileRecordModel model;
    verifySchemaMatchesSave(model);
}

void TestNodeConfigSchemas::networkSource_schemaCoversSaveKeys()
{
    NetworkSourceModel model;
    verifySchemaMatchesSave(model);
}

void TestNodeConfigSchemas::networkSink_schemaCoversSaveKeys()
{
    NetworkSinkModel model;
    verifySchemaMatchesSave(model);
}

void TestNodeConfigSchemas::gamepad_schemaCoversSaveKeys()
{
    GamepadModel model;
    verifySchemaMatchesSave(model);
}

void TestNodeConfigSchemas::plutoSdr_schemaCoversSaveKeys()
{
#ifdef HAVE_LIBIIO
    PlutoSdrModel model;
    verifySchemaMatchesSave(model);
#else
    QSKIP("PlutoSdrModel се компилира само при намерен libiio");
#endif
}

void TestNodeConfigSchemas::jackDetect_schemaCoversSaveKeys()
{
    JackDetectModel model;
    verifySchemaMatchesSave(model);
}

void TestNodeConfigSchemas::systemMonitor_schemaCoversSaveKeys()
{
    SystemMonitorModel model;
    verifySchemaMatchesSave(model);
}

void TestNodeConfigSchemas::gpuMonitor_schemaCoversSaveKeys()
{
    GpuMonitorModel model;
    verifySchemaMatchesSave(model);
}
