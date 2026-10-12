#pragma once

#include <QtWidgets/QWidget>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QSpinBox>
#include <QtWidgets/QDoubleSpinBox>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QLabel>
#include <QtWidgets/QTextEdit>
#include <QtWidgets/QTabWidget>
#include <QtWidgets/QCheckBox>
#include <QJsonArray>
#include <QJsonObject>

class ChatBaseWidget;

/**
 * @brief Server / debug / chat tabs of the LLaMA node (REQ-SW-PL-051).
 *
 * Pure presentation: every control reports a user action through a signal and
 * renders model state through a slot. The HTTP client, the QProcess and the
 * chat sessions all live in the core LLamaModelDataModel.
 */
class LLamaModelWidget : public QWidget
{
    Q_OBJECT

public:
    explicit LLamaModelWidget(QWidget *parent = nullptr);
    ~LLamaModelWidget() override = default;

public slots:
    void setStatus(QString const& status, bool connected);
    void setConnectButton(QString const& text, bool enabled);
    void setStartButton(QString const& text, QString const& styleSheet);
    void setDebugResponse(QString const& result);
    /// A chat response arrived — routed to the ChatBaseWidget.
    void addChatResponse(QString const& content, QJsonObject const& rawJson);

    // ── Initial render, driven by the core model's state ────────────────
    void setHost(QString const& host);
    void setPort(int port);
    void setExePath(QString const& path);
    void setModelPath(QString const& path);
    void setCtxSize(int ctxSize);
    void setUseGpu(bool useGpu);
    /// Access to the embedded chat, for config hydration.
    ChatBaseWidget* chatWidget() const { return m_chatWidget; }

signals:
    void connectClicked();
    void startServerClicked();
    void debugSendClicked();
    void chatSendRequested(QString const& text, QJsonArray const& messages,
                           double temperature, int nPredict);
    void exePathSelected(QString const& path);
    void modelPathSelected(QString const& path);
    void debugPathChanged(QString const& path);
    void debugBodyChanged(QString const& body);
    void chatConfigChanged(QJsonObject const& config);

private:
    QWidget* createServerTab();
    QWidget* createDebugTab();
    QWidget* createChatTab();
    void updateStartButtonEnabled();

    QTabWidget* m_tabWidget = nullptr;

    // Server tab
    QLineEdit* m_hostEdit = nullptr;
    QSpinBox* m_portSpin = nullptr;
    QPushButton* m_connectBtn = nullptr;
    QLabel* m_statusLabel = nullptr;
    QLineEdit* m_exePath = nullptr;
    QLineEdit* m_modelPath = nullptr;
    QSpinBox* m_ctxSizeSpin = nullptr;
    QCheckBox* m_useGpuCheck = nullptr;
    QPushButton* m_startBtn = nullptr;
    QPushButton* m_browseExeBtn = nullptr;
    QPushButton* m_browseModelBtn = nullptr;

    // Debug tab
    QLineEdit* m_debugPath = nullptr;
    QTextEdit* m_debugBody = nullptr;
    QPushButton* m_debugSendBtn = nullptr;
    QTextEdit* m_debugResponse = nullptr;

    // Chat tab
    ChatBaseWidget* m_chatWidget = nullptr;
};
