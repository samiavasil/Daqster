#pragma once

#include <QtWidgets/QWidget>
#include <QtWidgets/QListWidget>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QPlainTextEdit>
#include <QtWidgets/QTextEdit>
#include <QtWidgets/QTreeWidget>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QDoubleSpinBox>
#include <QtWidgets/QSpinBox>
#include <QtWidgets/QSplitter>

#include <QJsonArray>
#include <QJsonObject>

class ChatBaseWidget : public QWidget {
    Q_OBJECT
public:
    explicit ChatBaseWidget(QWidget* parent = nullptr);

    // Called by host when a response arrives
    void addResponse(QString const& content);

    // Append raw JSON to the debug tree
    void appendJsonToTree(QJsonObject const& obj, bool isSent);

    // Save/load sessions and config
    QJsonObject saveConfig() const;
    void loadConfig(QJsonObject const& p);
    QJsonArray sessions() const { return m_sessions; }
    void setSessions(QJsonArray const& s) { m_sessions = s; }
    QString activeSessionId() const { return m_activeSessionId; }
    void setActiveSessionId(QString const& id) { m_activeSessionId = id; }

    // Current config values
    QString systemPrompt() const;
    double temperature() const;
    int nPredict() const;

    // Set config values
    void setSystemPrompt(QString const& p);
    void setTemperature(double t);
    void setNPredict(int n);

    // Show/hide config panel
    void setConfigVisible(bool visible);
    bool configVisible() const;

signals:
    void sendRequested(QString const& text, QJsonArray const& messages,
                        double temperature, int nPredict);

private Q_SLOTS:
    void onSendClicked();
    void onNewSessionClicked();
    void onDeleteSessionClicked();
    void onSessionChanged(int row);
    void onJsonToggled(bool checked);
    void onConfigToggled(bool checked);
    void onClearHistoryClicked();

private:
    void setupUi();
    void rebuildSessionList();
    void loadChatDisplayFromSession();
    void ensureSystemMessage(QJsonArray& messages) const;
    QJsonObject getActiveSession() const;
    QString activeSessionName() const;
    void setActiveSessionMessages(QJsonArray const& messages);
    void appendChat(QString const& sender, QString const& text);
    void addJsonValue(QTreeWidgetItem* const item, QString const& key, QJsonValue const& value);

    // Left panel
    QListWidget* m_sessionList;
    QPushButton* m_newSessionBtn;
    QPushButton* m_deleteSessionBtn;

    // Config (wrapped for visibility toggle)
    QWidget* m_configContainer;
    QPlainTextEdit* m_systemPromptEdit;
    QDoubleSpinBox* m_tempSpin;
    QSpinBox* m_nPredictSpin;

    // Chat display + JSON tree
    QSplitter* m_chatSplitter;
    QTextEdit* m_chatDisplay;
    QTreeWidget* m_jsonTree;
    QPushButton* m_jsonBtn;

    // Input
    QLineEdit* m_chatInput;
    QPushButton* m_sendBtn;
    QPushButton* m_configBtn;
    QPushButton* m_clearHistoryBtn;

    // State
    QJsonArray m_sessions;
    QString m_activeSessionId;
    int m_messageCounter;
};
