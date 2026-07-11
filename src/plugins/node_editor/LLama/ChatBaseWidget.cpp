#include "ChatBaseWidget.h"

#include <QtCore/QUuid>
#include <QtCore/QDateTime>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QGroupBox>
#include <QtWidgets/QLabel>
#include <QtWidgets/QHeaderView>

ChatBaseWidget::ChatBaseWidget(QWidget* parent)
    : QWidget(parent)
    , m_messageCounter(0)
{
    setupUi();

    // Default session
    QJsonObject defaultSession;
    defaultSession["id"] = QUuid::createUuid().toString(QUuid::WithoutBraces);
    defaultSession["name"] = "Сесия 1";
    defaultSession["created"] = QDateTime::currentDateTime().toString(Qt::ISODate);
    defaultSession["messages"] = QJsonArray();
    m_sessions.append(defaultSession);
    m_activeSessionId = defaultSession["id"].toString();
    rebuildSessionList();
}

void ChatBaseWidget::setupUi() {
    auto* mainLayout = new QHBoxLayout(this);
    mainLayout->setSpacing(4);
    mainLayout->setContentsMargins(0, 0, 0, 0);

    // --- Left panel: sessions ---
    auto* sidePanel = new QWidget();
    sidePanel->setMaximumWidth(180);
    auto* sideLayout = new QVBoxLayout(sidePanel);
    sideLayout->setContentsMargins(4, 4, 0, 4);
    sideLayout->setSpacing(4);

    sideLayout->addWidget(new QLabel("<b>Сесии</b>"));

    m_sessionList = new QListWidget();
    m_sessionList->setAlternatingRowColors(true);
    sideLayout->addWidget(m_sessionList, 1);

    auto* sessionBtnLayout = new QHBoxLayout();
    sessionBtnLayout->setSpacing(4);
    m_newSessionBtn = new QPushButton("+ New");
    m_newSessionBtn->setMaximumWidth(50);
    m_deleteSessionBtn = new QPushButton("🗑");
    m_deleteSessionBtn->setMaximumWidth(30);
    sessionBtnLayout->addWidget(m_newSessionBtn);
    sessionBtnLayout->addWidget(m_deleteSessionBtn);
    sessionBtnLayout->addStretch();
    sideLayout->addLayout(sessionBtnLayout);

    mainLayout->addWidget(sidePanel);

    // --- Right panel: config, chat, input ---
    auto* rightPanel = new QWidget();
    auto* rightLayout = new QVBoxLayout(rightPanel);
    rightLayout->setSpacing(4);
    rightLayout->setContentsMargins(0, 4, 4, 4);

    // --- Config area ---
    auto* configGroup = new QGroupBox("Конфигурация");
    auto* configLayout = new QVBoxLayout(configGroup);
    configLayout->setSpacing(4);

    configLayout->addWidget(new QLabel("System Prompt:"));
    m_systemPromptEdit = new QPlainTextEdit();
    m_systemPromptEdit->setPlaceholderText("System prompt за модела...");
    m_systemPromptEdit->setPlainText("Ти си полезен AI асистент. Отговаряй кратко и точно.");
    m_systemPromptEdit->setMinimumHeight(100);
    m_systemPromptEdit->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::MinimumExpanding);
    configLayout->addWidget(m_systemPromptEdit);

    auto* paramsLayout = new QHBoxLayout();
    paramsLayout->setSpacing(8);

    paramsLayout->addWidget(new QLabel("Temp:"));
    m_tempSpin = new QDoubleSpinBox();
    m_tempSpin->setRange(0.0, 2.0);
    m_tempSpin->setSingleStep(0.05);
    m_tempSpin->setValue(0.3);
    m_tempSpin->setDecimals(2);
    m_tempSpin->setMaximumWidth(70);
    paramsLayout->addWidget(m_tempSpin);

    paramsLayout->addWidget(new QLabel("Max tokens:"));
    m_nPredictSpin = new QSpinBox();
    m_nPredictSpin->setRange(1, 65536);
    m_nPredictSpin->setValue(512);
    m_nPredictSpin->setSingleStep(128);
    m_nPredictSpin->setMaximumWidth(80);
    paramsLayout->addWidget(m_nPredictSpin);

    paramsLayout->addStretch();

    m_clearHistoryBtn = new QPushButton("Изчисти");
    m_clearHistoryBtn->setMaximumWidth(80);
    paramsLayout->addWidget(m_clearHistoryBtn);

    configLayout->addLayout(paramsLayout);

    // Wrap config in container for visibility toggle
    m_configContainer = new QWidget();
    auto* configContainerLayout = new QVBoxLayout(m_configContainer);
    configContainerLayout->setContentsMargins(0, 0, 0, 0);
    configContainerLayout->addWidget(configGroup);
    rightLayout->addWidget(m_configContainer);

    // --- Chat display + JSON tree ---
    m_chatSplitter = new QSplitter(Qt::Vertical);

    m_chatDisplay = new QTextEdit();
    m_chatDisplay->setReadOnly(true);
    m_chatDisplay->setPlaceholderText("Чат с модела...");

    m_jsonTree = new QTreeWidget();
    QFont monoFont("Courier New", 9);
    monoFont.setStyleHint(QFont::Monospace);
    m_jsonTree->setFont(monoFont);
    m_jsonTree->setAlternatingRowColors(true);
    m_jsonTree->setAnimated(true);
    m_jsonTree->setIndentation(20);
    m_jsonTree->setVisible(false);
    m_jsonTree->setHeaderLabels({"JSON Комуникация"});
    m_jsonTree->header()->setStretchLastSection(true);

    m_chatSplitter->addWidget(m_chatDisplay);
    m_chatSplitter->addWidget(m_jsonTree);
    m_chatSplitter->setStretchFactor(0, 3);
    m_chatSplitter->setStretchFactor(1, 2);

    rightLayout->addWidget(m_chatSplitter, 1);

    // --- Input area ---
    auto* inputLayout = new QHBoxLayout();
    m_chatInput = new QLineEdit();
    m_chatInput->setPlaceholderText("Напишете съобщение...");
    m_sendBtn = new QPushButton("Изпрати");
    m_sendBtn->setMaximumWidth(80);

    m_jsonBtn = new QPushButton("Raw JSON");
    m_jsonBtn->setMaximumWidth(80);
    m_jsonBtn->setCheckable(true);

    m_configBtn = new QPushButton("⚙ Config");
    m_configBtn->setMaximumWidth(80);
    m_configBtn->setCheckable(true);

    inputLayout->addWidget(m_chatInput);
    inputLayout->addWidget(m_sendBtn);
    inputLayout->addWidget(m_configBtn);
    inputLayout->addWidget(m_jsonBtn);

    rightLayout->addLayout(inputLayout);

    mainLayout->addWidget(rightPanel, 1);

    // --- Connections ---
    connect(m_sendBtn, &QPushButton::clicked, this, &ChatBaseWidget::onSendClicked);
    connect(m_chatInput, &QLineEdit::returnPressed, this, &ChatBaseWidget::onSendClicked);
    connect(m_clearHistoryBtn, &QPushButton::clicked, this, &ChatBaseWidget::onClearHistoryClicked);
    connect(m_newSessionBtn, &QPushButton::clicked, this, &ChatBaseWidget::onNewSessionClicked);
    connect(m_deleteSessionBtn, &QPushButton::clicked, this, &ChatBaseWidget::onDeleteSessionClicked);
    connect(m_jsonBtn, &QPushButton::toggled, this, &ChatBaseWidget::onJsonToggled);
    connect(m_configBtn, &QPushButton::toggled, this, &ChatBaseWidget::onConfigToggled);
    connect(m_sessionList, &QListWidget::currentRowChanged, this, &ChatBaseWidget::onSessionChanged);
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------
void ChatBaseWidget::addResponse(QString const& content) {
    if (content.isEmpty())
        return;

    QJsonObject session = getActiveSession();
    QJsonArray msgs = session["messages"].toArray();
    QJsonObject asstMsg;
    asstMsg["role"] = "assistant";
    asstMsg["content"] = content;
    msgs.append(asstMsg);
    setActiveSessionMessages(msgs);
    rebuildSessionList();
}

void ChatBaseWidget::appendJsonToTree(QJsonObject const& obj, bool isSent) {
    m_messageCounter++;

    auto* rootItem = new QTreeWidgetItem(m_jsonTree);
    QFont boldFont = rootItem->font(0);
    boldFont.setBold(true);

    if (isSent) {
        rootItem->setText(0, QString("#%1  ИЗПРАТЕНО  →").arg(m_messageCounter));
        rootItem->setForeground(0, QColor(0, 0, 200));
    } else {
        rootItem->setText(0, QString("#%1  ОТГОВОР  ←").arg(m_messageCounter));
        rootItem->setForeground(0, QColor(200, 0, 0));
    }
    rootItem->setFont(0, boldFont);
    rootItem->setExpanded(true);

    for (auto it = obj.begin(); it != obj.end(); ++it) {
        auto* child = new QTreeWidgetItem(rootItem);
        addJsonValue(child, it.key(), it.value());
    }

    m_jsonTree->scrollToBottom();
}

QJsonObject ChatBaseWidget::saveConfig() const {
    QJsonObject obj;
    obj["systemPrompt"] = m_systemPromptEdit->toPlainText();
    obj["temperature"] = m_tempSpin->value();
    obj["nPredict"] = m_nPredictSpin->value();
    obj["sessions"] = m_sessions;
    obj["activeSessionId"] = m_activeSessionId;
    return obj;
}

void ChatBaseWidget::loadConfig(QJsonObject const& p) {
    if (p.contains("systemPrompt"))
        m_systemPromptEdit->setPlainText(p["systemPrompt"].toString());
    if (p.contains("temperature"))
        m_tempSpin->setValue(p["temperature"].toDouble());
    if (p.contains("nPredict"))
        m_nPredictSpin->setValue(p["nPredict"].toInt());
    if (p.contains("sessions"))
        m_sessions = p["sessions"].toArray();
    if (p.contains("activeSessionId"))
        m_activeSessionId = p["activeSessionId"].toString();

    if (m_sessions.isEmpty()) {
        QJsonObject defaultSession;
        defaultSession["id"] = QUuid::createUuid().toString(QUuid::WithoutBraces);
        defaultSession["name"] = "Сесия 1";
        defaultSession["created"] = QDateTime::currentDateTime().toString(Qt::ISODate);
        defaultSession["messages"] = QJsonArray();
        m_sessions.append(defaultSession);
        m_activeSessionId = defaultSession["id"].toString();
    }

    rebuildSessionList();
}

QString ChatBaseWidget::systemPrompt() const {
    return m_systemPromptEdit ? m_systemPromptEdit->toPlainText().trimmed() : "";
}

double ChatBaseWidget::temperature() const {
    return m_tempSpin ? m_tempSpin->value() : 0.3;
}

int ChatBaseWidget::nPredict() const {
    return m_nPredictSpin ? m_nPredictSpin->value() : 512;
}

void ChatBaseWidget::setSystemPrompt(QString const& p) {
    if (m_systemPromptEdit)
        m_systemPromptEdit->setPlainText(p);
}

void ChatBaseWidget::setTemperature(double t) {
    if (m_tempSpin)
        m_tempSpin->setValue(t);
}

void ChatBaseWidget::setNPredict(int n) {
    if (m_nPredictSpin)
        m_nPredictSpin->setValue(n);
}

void ChatBaseWidget::setConfigVisible(bool visible) {
    if (m_configContainer)
        m_configContainer->setVisible(visible);
    if (m_configBtn) {
        m_configBtn->blockSignals(true);
        m_configBtn->setChecked(visible);
        m_configBtn->blockSignals(false);
    }
}

bool ChatBaseWidget::configVisible() const {
    return m_configContainer ? m_configContainer->isVisible() : true;
}

// ---------------------------------------------------------------------------
// Slots
// ---------------------------------------------------------------------------
void ChatBaseWidget::onSendClicked() {
    QString text = m_chatInput->text().trimmed();
    if (text.isEmpty())
        return;

    QJsonObject session = getActiveSession();
    QJsonArray messages = session["messages"].toArray();

    ensureSystemMessage(messages);

    QJsonObject userMsg;
    userMsg["role"] = "user";
    userMsg["content"] = text;
    messages.append(userMsg);

    setActiveSessionMessages(messages);
    rebuildSessionList();

    QJsonObject requestPreview;
    requestPreview["messages"] = messages;
    requestPreview["temperature"] = m_tempSpin->value();
    requestPreview["n_predict"] = m_nPredictSpin->value();
    requestPreview["stream"] = false;
    appendJsonToTree(requestPreview, true);

    m_chatInput->clear();

    Q_EMIT sendRequested(text, messages, m_tempSpin->value(), m_nPredictSpin->value());
}

void ChatBaseWidget::onNewSessionClicked() {
    QJsonObject session;
    session["id"] = QUuid::createUuid().toString(QUuid::WithoutBraces);
    session["name"] = QString("Сесия %1").arg(m_sessions.size() + 1);
    session["created"] = QDateTime::currentDateTime().toString(Qt::ISODate);
    session["messages"] = QJsonArray();
    m_sessions.append(session);
    m_activeSessionId = session["id"].toString();
    rebuildSessionList();
    m_chatDisplay->clear();
    m_jsonTree->clear();
}

void ChatBaseWidget::onDeleteSessionClicked() {
    if (m_sessions.size() <= 1)
        return;

    int row = m_sessionList->currentRow();
    if (row < 0 || row >= m_sessions.size())
        return;

    QJsonObject current = m_sessions[row].toObject();
    m_sessions.removeAt(row);

    if (current["id"].toString() == m_activeSessionId) {
        int newRow = qMin(row, m_sessions.size() - 1);
        m_activeSessionId = m_sessions[newRow].toObject()["id"].toString();
    }

    rebuildSessionList();
    m_jsonTree->clear();
}

void ChatBaseWidget::onSessionChanged(int row) {
    if (row < 0 || row >= m_sessions.size())
        return;

    QJsonObject session = m_sessions[row].toObject();
    m_activeSessionId = session["id"].toString();
    loadChatDisplayFromSession();
}

void ChatBaseWidget::onJsonToggled(bool checked) {
    m_jsonTree->setVisible(checked);
}

void ChatBaseWidget::onConfigToggled(bool checked) {
    if (m_configContainer)
        m_configContainer->setVisible(checked);
}

void ChatBaseWidget::onClearHistoryClicked() {
    // Clear current session messages but keep the session
    setActiveSessionMessages(QJsonArray());
    m_chatDisplay->clear();
    m_jsonTree->clear();
    rebuildSessionList();
}

// ---------------------------------------------------------------------------
// Session helpers
// ---------------------------------------------------------------------------
QJsonObject ChatBaseWidget::getActiveSession() const {
    for (auto const& s : m_sessions) {
        QJsonObject sess = s.toObject();
        if (sess["id"].toString() == m_activeSessionId)
            return sess;
    }
    return QJsonObject();
}

QString ChatBaseWidget::activeSessionName() const {
    QJsonObject s = getActiveSession();
    return QString("%1 (%2)").arg(s["name"].toString()).arg(s["messages"].toArray().size());
}

void ChatBaseWidget::setActiveSessionMessages(QJsonArray const& messages) {
    for (int i = 0; i < m_sessions.size(); ++i) {
        QJsonObject sess = m_sessions[i].toObject();
        if (sess["id"].toString() == m_activeSessionId) {
            sess["messages"] = messages;
            m_sessions[i] = sess;
            return;
        }
    }
}

void ChatBaseWidget::ensureSystemMessage(QJsonArray& messages) const {
    QString sysContent = systemPrompt();
    if (sysContent.isEmpty())
        sysContent = "Ти си полезен AI асистент.";

    for (int i = messages.size() - 1; i >= 0; --i) {
        if (messages[i].toObject()["role"].toString() == "system") {
            messages.removeAt(i);
        }
    }

    QJsonObject sysMsg;
    sysMsg["role"] = "system";
    sysMsg["content"] = sysContent;
    messages.prepend(sysMsg);
}

void ChatBaseWidget::rebuildSessionList() {
    m_sessionList->blockSignals(true);
    m_sessionList->clear();

    int activeRow = -1;
    for (int i = 0; i < m_sessions.size(); ++i) {
        QJsonObject s = m_sessions[i].toObject();
        auto* item = new QListWidgetItem(activeSessionName());
        m_sessionList->addItem(item);

        if (s["id"].toString() == m_activeSessionId)
            activeRow = i;
    }

    m_sessionList->blockSignals(false);

    if (activeRow >= 0) {
        m_sessionList->setCurrentRow(activeRow);
        loadChatDisplayFromSession();
    }
}

void ChatBaseWidget::loadChatDisplayFromSession() {
    m_chatDisplay->clear();

    QJsonObject session = getActiveSession();
    QJsonArray messages = session["messages"].toArray();

    for (auto const& msgVal : messages) {
        QJsonObject msg = msgVal.toObject();
        QString role = msg["role"].toString();
        QString content = msg["content"].toString();

        if (role == "system")
            continue;

        QString sender = (role == "user") ? "Вие" : "Модел";
        appendChat(sender, content);
    }
}

void ChatBaseWidget::appendChat(QString const& sender, QString const& text) {
    m_chatDisplay->append(QString("<b>%1:</b>").arg(sender));
    m_chatDisplay->append(text);
    m_chatDisplay->append("");
}

// ---------------------------------------------------------------------------
// JSON tree helpers
// ---------------------------------------------------------------------------
void ChatBaseWidget::addJsonValue(QTreeWidgetItem* item, QString const& key, QJsonValue const& value) {
    switch (value.type()) {
    case QJsonValue::Object: {
        QJsonObject childObj = value.toObject();
        item->setText(0, QString("%1  {%2}").arg(key).arg(childObj.size()));
        item->setChildIndicatorPolicy(QTreeWidgetItem::ShowIndicator);
        for (auto it = childObj.begin(); it != childObj.end(); ++it) {
            auto* child = new QTreeWidgetItem(item);
            addJsonValue(child, it.key(), it.value());
        }
        break;
    }
    case QJsonValue::Array: {
        QJsonArray arr = value.toArray();
        item->setText(0, QString("%1  [%2]").arg(key).arg(arr.size()));
        item->setChildIndicatorPolicy(QTreeWidgetItem::ShowIndicator);
        for (int i = 0; i < arr.size(); ++i) {
            auto* child = new QTreeWidgetItem(item);
            addJsonValue(child, QString("[%1]").arg(i), arr.at(i));
        }
        break;
    }
    case QJsonValue::String:
        item->setText(0, QString("%1: \"%2\"").arg(key, value.toString()));
        break;
    case QJsonValue::Double:
        item->setText(0, QString("%1: %2").arg(key).arg(value.toDouble(), 0, 'g', 10));
        break;
    case QJsonValue::Bool:
        item->setText(0, QString("%1: %2").arg(key).arg(value.toBool() ? "true" : "false"));
        break;
    case QJsonValue::Null:
        item->setText(0, QString("%1: null").arg(key));
        break;
    default:
        item->setText(0, QString("%1: undefined").arg(key));
        break;
    }
}
