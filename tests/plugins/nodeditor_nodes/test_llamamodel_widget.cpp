#include "test_llamamodel_widget.h"

#include "ChatBaseWidget.h"
#include "LLamaModelWidget.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QLineEdit>
#include <QPushButton>
#include <QTreeWidget>
#include <QtTest/QtTest>

namespace {

QLineEdit* exeEdit(LLamaModelWidget& w)
{
    auto* e = w.findChild<QLineEdit*>(QStringLiteral("exePathEdit"));
    Q_ASSERT(e != nullptr);
    return e;
}

QLineEdit* modelEdit(LLamaModelWidget& w)
{
    auto* e = w.findChild<QLineEdit*>(QStringLiteral("modelPathEdit"));
    Q_ASSERT(e != nullptr);
    return e;
}

QPushButton* startButton(LLamaModelWidget& w)
{
    auto* b = w.findChild<QPushButton*>(QStringLiteral("startServerButton"));
    Q_ASSERT(b != nullptr);
    return b;
}

} // namespace

// A fresh node has no paths, so its local server cannot be started.
void TestLlamaModelWidget::startButton_disabledOnFreshWidget()
{
    LLamaModelWidget w;
    QVERIFY2(!startButton(w)->isEnabled(),
             "start button must be disabled while both paths are empty");
}

void TestLlamaModelWidget::startButton_disabledWithOnlyExePath()
{
    LLamaModelWidget w;
    exeEdit(w)->setText(QStringLiteral("/usr/bin/llama-server"));
    QVERIFY(!startButton(w)->isEnabled());
}

void TestLlamaModelWidget::startButton_disabledWithOnlyModelPath()
{
    LLamaModelWidget w;
    modelEdit(w)->setText(QStringLiteral("/models/llama-3.2-3b.gguf"));
    QVERIFY(!startButton(w)->isEnabled());
}

// The regression: typing into both fields (bypassing setExePath/setModelPath)
// must still enable the button.
void TestLlamaModelWidget::startButton_enabledWhenBothPathsTyped()
{
    LLamaModelWidget w;
    QVERIFY(!startButton(w)->isEnabled());

    exeEdit(w)->setText(QStringLiteral("/usr/bin/llama-server"));
    QVERIFY(!startButton(w)->isEnabled());

    modelEdit(w)->setText(QStringLiteral("/models/llama-3.2-3b.gguf"));
    QVERIFY2(startButton(w)->isEnabled(),
             "both paths set by typing -> start button must be enabled");
}

void TestLlamaModelWidget::startButton_disabledAgainWhenPathCleared()
{
    LLamaModelWidget w;
    exeEdit(w)->setText(QStringLiteral("/usr/bin/llama-server"));
    modelEdit(w)->setText(QStringLiteral("/models/llama-3.2-3b.gguf"));
    QVERIFY(startButton(w)->isEnabled());

    modelEdit(w)->setText(QString());
    QVERIFY(!startButton(w)->isEnabled());
}

// Whitespace-only paths do not count as "set".
void TestLlamaModelWidget::startButton_reflectsSetters()
{
    LLamaModelWidget w;

    // The factory hydrates a brand-new node with empty paths — idle state.
    w.setExePath(QString());
    w.setModelPath(QString());
    QVERIFY(!startButton(w)->isEnabled());

    w.setExePath(QStringLiteral("/usr/bin/llama-server"));
    QVERIFY(!startButton(w)->isEnabled());

    w.setModelPath(QStringLiteral("/models/llama-3.2-3b.gguf"));
    QVERIFY(startButton(w)->isEnabled());

    // A whitespace-only value must not enable the button.
    w.setModelPath(QStringLiteral("   "));
    QVERIFY(!startButton(w)->isEnabled());
}

// AC 5: a raw exchange (any origin) lands in the JSON tree only; it must NOT
// add an assistant bubble to the node's own chat session. That is exactly the
// regression where a Конзола-driven request polluted LLaMA Model's chat.
void TestLlamaModelWidget::appendExchange_addsTreeEntryWithoutChatBubble()
{
    LLamaModelWidget w;

    auto* chat = w.chatWidget();
    QVERIFY(chat != nullptr);

    auto* tree = w.findChild<QTreeWidget*>();
    QVERIFY(tree != nullptr);

    const int treeBefore = tree->topLevelItemCount();
    const QByteArray sessionsBefore =
        QJsonDocument(chat->sessions()).toJson(QJsonDocument::Compact);

    QJsonObject exchange;
    exchange[QStringLiteral("request_body")] =
        QJsonObject{{QStringLiteral("model"), QStringLiteral("local")}};
    exchange[QStringLiteral("response")] =
        QJsonObject{{QStringLiteral("choices"), QJsonArray{{QStringLiteral("x")}}}};

    w.appendExchange(exchange);

    QCOMPARE(tree->topLevelItemCount(), treeBefore + 1);
    QCOMPARE(QJsonDocument(chat->sessions()).toJson(QJsonDocument::Compact), sessionsBefore);

    // An empty exchange is a no-op.
    w.appendExchange(QJsonObject());
    QCOMPARE(tree->topLevelItemCount(), treeBefore + 1);
    QCOMPARE(QJsonDocument(chat->sessions()).toJson(QJsonDocument::Compact), sessionsBefore);
}

// The complement of the above: a local-chat reply is the only thing that adds a
// bubble to the session.
void TestLlamaModelWidget::addChatResponse_addsChatBubble()
{
    LLamaModelWidget w;

    auto* chat = w.chatWidget();
    QVERIFY(chat != nullptr);
    QVERIFY(!chat->sessions().isEmpty());

    const QJsonObject before = chat->sessions().at(0).toObject();
    const int messagesBefore = before.value(QStringLiteral("messages")).toArray().size();

    w.addChatResponse(QStringLiteral("pong"), QJsonObject());

    const QJsonObject after = chat->sessions().at(0).toObject();
    QCOMPARE(after.value(QStringLiteral("messages")).toArray().size(), messagesBefore + 1);

    // An empty response is a no-op (guarded in ChatBaseWidget::addResponse).
    const QByteArray sessionsAfter =
        QJsonDocument(chat->sessions()).toJson(QJsonDocument::Compact);
    w.addChatResponse(QString(), QJsonObject());
    QCOMPARE(QJsonDocument(chat->sessions()).toJson(QJsonDocument::Compact), sessionsAfter);
}

QTEST_MAIN(TestLlamaModelWidget)
