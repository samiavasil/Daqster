#include "ConsoleDataModel.h"

using QtNodes::PortType;
using QtNodes::PortIndex;
using QtNodes::NodeData;
using QtNodes::NodeDataType;
using QtNodes::NodeDelegateModel;

ConsoleDataModel::ConsoleDataModel()
    : m_outputText(std::make_shared<TextData>(""))
{
}

unsigned int ConsoleDataModel::nPorts(PortType portType) const
{
    switch (portType) {
    case PortType::In:
        return 1;
    case PortType::Out:
        return 1;
    default:
        return 0;
    }
}

NodeDataType ConsoleDataModel::dataType(PortType, PortIndex) const
{
    return TextData().type();
}

std::shared_ptr<NodeData> ConsoleDataModel::outData(PortIndex const)
{
    return m_outputText;
}

void ConsoleDataModel::setInData(std::shared_ptr<NodeData> data, PortIndex const)
{
    if (auto textData = std::dynamic_pointer_cast<TextData>(data)) {
        QString raw = textData->text();

        QJsonDocument doc = QJsonDocument::fromJson(raw.toUtf8());
        if (doc.isObject()) {
            QJsonObject obj = doc.object();

            // Extract the content of response.message. REQ-SW-PL-074 (R6): a
            // "thinking" model (Qwen3, QwQ, DeepSeek-R1 …) returns an empty
            // content with the text in reasoning_content, so fall back to that
            // (and the /completion "text" shape) rather than showing nothing.
            auto pickContent = [](QJsonObject const& msg) -> QString {
                QString c = msg["content"].toString();
                if (c.isEmpty())
                    c = msg["reasoning_content"].toString();
                return c;
            };

            QString content;
            if (obj.contains("response")) {
                QJsonObject resp = obj["response"].toObject();
                QJsonArray choices = resp["choices"].toArray();
                if (!choices.isEmpty()) {
                    QJsonObject choice = choices.at(0).toObject();
                    content = pickContent(choice["message"].toObject());
                    if (content.isEmpty())
                        content = choice["text"].toString(); // /completion shape
                }
            } else if (obj.contains("message")) {
                content = pickContent(obj["message"].toObject());
            } else if (obj.contains("content")) {
                content = obj["content"].toString();
            }

            if (!content.isEmpty())
                Q_EMIT responseReceived(content, obj);
        }
    }
}

void ConsoleDataModel::onSendClicked(QString const& text, QJsonArray const& messages,
                                     double temperature, int nPredict)
{
    Q_UNUSED(text)

    QJsonObject msg;
    msg["messages"] = messages;
    msg["temperature"] = temperature;
    msg["n_predict"] = nPredict;

    QString jsonStr = QString::fromUtf8(QJsonDocument(msg).toJson(QJsonDocument::Compact));
    m_outputText = std::make_shared<TextData>(jsonStr);
    Q_EMIT dataUpdated(0);
}

void ConsoleDataModel::onChatConfigChanged(QJsonObject const& config)
{
    m_chatConfig = config;
}

QJsonObject ConsoleDataModel::save() const
{
    QJsonObject obj = NodeDelegateModel::save();
    for (auto it = m_chatConfig.begin(); it != m_chatConfig.end(); ++it)
        obj[it.key()] = it.value();
    return obj;
}

void ConsoleDataModel::load(QJsonObject const& p)
{
    m_chatConfig = p;
}
