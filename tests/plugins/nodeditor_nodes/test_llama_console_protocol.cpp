#include "test_llama_console_protocol.h"

#include "ConsoleDataModel.h"
#include "LLamaModelDataModel.h"
#include "NodeDataTypes/TextData.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSignalSpy>
#include <QtTest/QtTest>

#include <memory>

namespace {

std::shared_ptr<TextData> textData(QString const& text)
{
    return std::make_shared<TextData>(text);
}

QJsonObject consoleRequestEnvelope(QString const& prompt)
{
    QJsonArray messages;
    messages.append(QJsonObject{{QStringLiteral("role"), QStringLiteral("user")},
                                {QStringLiteral("content"), prompt}});
    QJsonObject envelope;
    envelope[QStringLiteral("messages")] = messages;
    envelope[QStringLiteral("temperature")] = 0.7;
    envelope[QStringLiteral("n_predict")] = 64;
    return envelope;
}

QJsonObject llamaModelResponseEnvelope(QString const& content)
{
    QJsonObject choice;
    choice[QStringLiteral("message")] = QJsonObject{{QStringLiteral("role"), QStringLiteral("assistant")},
                                                    {QStringLiteral("content"), content}};
    QJsonArray choices;
    choices.append(choice);

    QJsonObject response;
    response[QStringLiteral("choices")] = choices;

    QJsonObject envelope;
    envelope[QStringLiteral("request_body")] = consoleRequestEnvelope(QStringLiteral("ping"));
    envelope[QStringLiteral("response")] = response;
    return envelope;
}

QString compact(QJsonObject const& obj)
{
    return QString::fromUtf8(QJsonDocument(obj).toJson(QJsonDocument::Compact));
}

} // namespace

// AC 2: what the Конзола puts on its output port is exactly what LlamaModel's
// setInData() reads back.
void TestLlamaConsoleProtocol::consoleEmitsRequestEnvelopeWithModelSchema()
{
    ConsoleDataModel console;
    QJsonArray messages;
    messages.append(QJsonObject{{QStringLiteral("role"), QStringLiteral("user")},
                                {QStringLiteral("content"), QStringLiteral("hello")}});

    console.onSendClicked(QStringLiteral("hello"), messages, 0.7, 64);

    auto data = std::dynamic_pointer_cast<TextData>(console.outData(0));
    QVERIFY(data != nullptr);

    QJsonDocument doc = QJsonDocument::fromJson(data->text().toUtf8());
    QVERIFY(doc.isObject());
    QJsonObject obj = doc.object();

    QVERIFY(obj.contains(QStringLiteral("messages")));
    QVERIFY(obj.contains(QStringLiteral("temperature")));
    QVERIFY(obj.contains(QStringLiteral("n_predict")));
    QCOMPARE(obj.value(QStringLiteral("messages")).toArray().size(), 1);
    QCOMPARE(obj.value(QStringLiteral("temperature")).toDouble(), 0.7);
    QCOMPARE(obj.value(QStringLiteral("n_predict")).toInt(), 64);
}

// AC 2: LlamaModel parses the Конзола envelope. With no server connection the
// request cannot be served, but parsing must not crash and the model must not
// silently treat the input as junk.
void TestLlamaConsoleProtocol::llamaModelAcceptsConsoleEnvelope()
{
    LLamaModelDataModel model;
    QSignalSpy statusSpy(&model, &LLamaModelDataModel::statusChanged);

    model.setInData(textData(compact(consoleRequestEnvelope(QStringLiteral("ping")))), 0);

    // No connection → one diagnostic status (AC 4), not a silent drop.
    QVERIFY(statusSpy.count() >= 1);
}

// AC 4: a request that cannot be served must say so.
void TestLlamaConsoleProtocol::llamaModelReportsNoConnectionInsteadOfSilence()
{
    LLamaModelDataModel model;
    QSignalSpy statusSpy(&model, &LLamaModelDataModel::statusChanged);

    model.setInData(textData(compact(consoleRequestEnvelope(QStringLiteral("ping")))), 0);

    QCOMPARE(statusSpy.count(), 1);
    const QString status = statusSpy.first().at(0).toString();
    QVERIFY2(!status.isEmpty(), "the no-connection status must carry a message");
}

// AC 3: what LlamaModel publishes, the Конзола reads back into a reply.
void TestLlamaConsoleProtocol::consoleExtractsReplyFromLlamaModelEnvelope()
{
    ConsoleDataModel console;
    QSignalSpy responseSpy(&console, &ConsoleDataModel::responseReceived);

    console.setInData(textData(compact(llamaModelResponseEnvelope(QStringLiteral("pong")))), 0);

    QCOMPARE(responseSpy.count(), 1);
    QCOMPARE(responseSpy.first().at(0).toString(), QStringLiteral("pong"));
    // The raw envelope rides along for downstream display.
    QVERIFY(responseSpy.first().at(1).toJsonObject().contains(QStringLiteral("response")));
}

// The Конзола also tolerates the slimmer {"message":{...}} / {"content":...}
// shapes (legacy / debug responses).
void TestLlamaConsoleProtocol::consoleAlsoAcceptsMessageAndContentShapes()
{
    {
        ConsoleDataModel console;
        QSignalSpy spy(&console, &ConsoleDataModel::responseReceived);
        QJsonObject obj{{QStringLiteral("message"),
                         QJsonObject{{QStringLiteral("content"), QStringLiteral("from-message")}}}};
        console.setInData(textData(compact(obj)), 0);
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.first().at(0).toString(), QStringLiteral("from-message"));
    }
    {
        ConsoleDataModel console;
        QSignalSpy spy(&console, &ConsoleDataModel::responseReceived);
        QJsonObject obj{{QStringLiteral("content"), QStringLiteral("from-content")}};
        console.setInData(textData(compact(obj)), 0);
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.first().at(0).toString(), QStringLiteral("from-content"));
    }
}

void TestLlamaConsoleProtocol::llamaModelIgnoresEmptyAndMalformedInput()
{
    LLamaModelDataModel model;

    {
        QSignalSpy statusSpy(&model, &LLamaModelDataModel::statusChanged);
        model.setInData(nullptr, 0);
        model.setInData(textData(QString()), 0);
        model.setInData(textData(QStringLiteral("not json at all")), 0);
        model.setInData(textData(QStringLiteral("{\"messages\":[]}")), 0);
        QCOMPARE(statusSpy.count(), 0);
    }
}

QTEST_GUILESS_MAIN(TestLlamaConsoleProtocol)
