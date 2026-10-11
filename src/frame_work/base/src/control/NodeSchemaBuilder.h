#pragma once

#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QString>

namespace Daqster {

/**
 * @brief Best-effort JSON Schema Draft 7 fragment for one saved JSON value.
 *
 * The model's `save()` output is the authoritative shape; without per-node
 * metadata this is the strongest schema that can be derived (REQ-SW-PL-054
 * AC3). Shared by the headless engine and the GUI runtime host so the
 * `/api/nodes/{id}/schema` endpoint answers identically in both modes.
 */
inline QJsonObject jsonSchemaForValue(const QJsonValue& value)
{
    QJsonObject schema;
    switch (value.type()) {
    case QJsonValue::Bool:
        schema.insert(QStringLiteral("type"), QStringLiteral("boolean"));
        break;
    case QJsonValue::Double:
        schema.insert(QStringLiteral("type"), QStringLiteral("number"));
        break;
    case QJsonValue::String:
        schema.insert(QStringLiteral("type"), QStringLiteral("string"));
        break;
    case QJsonValue::Array: {
        schema.insert(QStringLiteral("type"), QStringLiteral("array"));
        const QJsonArray array = value.toArray();
        if (!array.isEmpty())
            schema.insert(QStringLiteral("items"), jsonSchemaForValue(array.first()));
        break;
    }
    case QJsonValue::Object: {
        schema.insert(QStringLiteral("type"), QStringLiteral("object"));
        QJsonObject properties;
        QJsonArray required;
        const QJsonObject object = value.toObject();
        for (auto it = object.constBegin(); it != object.constEnd(); ++it) {
            properties.insert(it.key(), jsonSchemaForValue(it.value()));
            required.append(it.key());
        }
        schema.insert(QStringLiteral("properties"), properties);
        if (!required.isEmpty())
            schema.insert(QStringLiteral("required"), required);
        break;
    }
    default:
        schema.insert(QStringLiteral("type"), QStringLiteral("null"));
        break;
    }
    return schema;
}

/**
 * @brief A complete Draft-7 schema for a node type's `save()` output.
 *
 * @param defaults `NodeDelegateModel::save()` of a throw-away model instance.
 * @param title    Node type name (`NodeDelegateModel::name()`).
 */
inline QJsonObject nodeConfigSchema(const QJsonObject& defaults, const QString& title)
{
    QJsonObject schema = jsonSchemaForValue(defaults);
    schema.insert(QStringLiteral("$schema"),
                  QStringLiteral("http://json-schema.org/draft-07/schema#"));
    schema.insert(QStringLiteral("title"), title);
    return schema;
}

} // namespace Daqster
