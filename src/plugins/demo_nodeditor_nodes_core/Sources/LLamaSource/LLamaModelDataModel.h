#pragma once

#include <QtCore/QObject>
#include <QtCore/QProcess>
#include <QtNetwork/QNetworkAccessManager>
#include <QtNetwork/QNetworkReply>
#include <QJsonArray>

#include <QtNodes/NodeDelegateModel>

#include <memory>
#include <functional>

#include "NodeDataTypes/TextData.h"
#include "shared/IStoppable.h"
#include "shared/IStartable.h"

/**
 * @brief LLaMA model node (REQ-SW-PL-024).
 *
 * Talks to a llama.cpp server over HTTP (existing server, or one this node
 * spawns locally as a QProcess). Owns NO widgets after REQ-SW-PL-051: the
 * server/debug/chat tabs live in the GUI plugin (LLamaModelWidget) and are
 * wired through NodeWidgetFactory. Every field the UI used to hold is a plain
 * model member here, user edits arrive through the public on* slots, and the
 * model reports state back through the signals below.
 */
class LLamaModelDataModel : public QtNodes::NodeDelegateModel, public Daqster::IStoppable, public Daqster::IStartable {
  Q_OBJECT

public:
  LLamaModelDataModel();
  virtual ~LLamaModelDataModel();

  QString caption() const override { return QStringLiteral("LLaMA Model"); }
  bool captionVisible() const override { return true; }
  QString name() const override { return QStringLiteral("LLamaModel"); }

  unsigned int nPorts(QtNodes::PortType portType) const override;
  QtNodes::NodeDataType dataType(QtNodes::PortType portType, QtNodes::PortIndex portIndex) const override;
  std::shared_ptr<QtNodes::NodeData> outData(QtNodes::PortIndex const port) override;
  void setInData(std::shared_ptr<QtNodes::NodeData> data, QtNodes::PortIndex const portIndex) override;

  /// Core model has no QtWidgets dependency (REQ-SW-PL-051).
  QWidget* embeddedWidget() override { return nullptr; }
  bool resizable() const override { return true; }

  /// Stop the local server process. Idempotent — safe to call multiple
  /// times (REQ-SW-PL-050).
  void stop() override;

  /// Start the local server process programmatically (runtime autoStart, REQ-SW-PL-048).
  void start() override;

  /// The node BODY (boundary, caption, ports) does not depend on data —
  /// widget content self-repaints via Qt. Opts out of the body repaint.
  bool dataArrivalChangesWidget() const override { return false; }

  QJsonObject save() const override;
  void load(QJsonObject const& p) override;

  // ── Configuration state, for the GUI plugin to render the widget from ──
  QString host() const { return m_host; }
  int port() const { return m_port; }
  QString exePath() const { return m_exePath; }
  QString modelPath() const { return m_modelPath; }
  int ctxSize() const { return m_ctxSize; }
  bool useGpu() const { return m_useGpu; }
  QString debugPath() const { return m_debugPath; }
  QString debugBody() const { return m_debugBody; }
  bool connected() const { return m_connected; }
  /// Persisted chat state mirrored from the widget (sessions, prompt, ...).
  QJsonObject chatConfig() const { return m_chatConfig; }

public Q_SLOTS:
  /// Controls of LLamaModelWidget (called by NodeWidgetFactory).
  void onConnectClicked();
  void onStartServerClicked();
  void onDebugSendClicked();
  void onLocalChatSend(QString const& text, QJsonArray const& messages,
                       double temperature, int nPredict);
  /// A path was picked in a file dialog inside the widget.
  void onExePathSelected(QString const& path);
  void onModelPathSelected(QString const& path);
  void onDebugPathChanged(QString const& path);
  void onDebugBodyChanged(QString const& body);
  /// The widget's persisted chat state (REQ-SW-PL-051).
  void onChatConfigChanged(QJsonObject const& config);

signals:
  /// Status line text + connected colouring.
  void statusChanged(QString const& status, bool connected);
  /// Connect button label ("Свържи" / "Изключи") and enabled state.
  void connectButtonChanged(QString const& text, bool enabled);
  /// Start/stop server button label + stylesheet.
  void startButtonChanged(QString const& text, QString const& styleSheet);
  /// Raw debug request/response exchange, appended to the JSON tree.
  void chatResponseReceived(QString const& content, QJsonObject const& rawJson);
  void debugResponseReceived(QString const& result);

private Q_SLOTS:
  void onProcessStarted();
  void onProcessFinished(int exitCode, QProcess::ExitStatus status);
  void onProcessStdout();
  void onProcessStderr();

private:
  void checkHealth();
  void onHealthReply(bool healthy, QString const& error);

  // Stateless send - accepts messages[] array directly
  void sendToModel(QJsonArray const& messages, double temperature, int nPredict,
                   std::function<void(QString)> onResult = nullptr);

  void setStatus(QString const& status, bool connected);

  // ── Server tab state ────────────────────────────────────────────────
  QString m_host = QStringLiteral("127.0.0.1");
  int m_port = 8080;
  QString m_exePath;
  QString m_modelPath;
  int m_ctxSize = 2048;
  bool m_useGpu = true;

  // ── Debug tab state ─────────────────────────────────────────────────
  QString m_debugPath = QStringLiteral("/completion");
  QString m_debugBody;

  // ── Persisted chat state (mirrored from ChatBaseWidget) ─────────────
  QJsonObject m_chatConfig;

  // Networking
  QNetworkAccessManager* m_netManager;
  QString m_baseUrl;
  bool m_connected = false;
  /// Health check in flight — suppresses a redundant status report.
  bool m_healthPending = false;

  // Local server
  QProcess* m_serverProcess = nullptr;

  // Data ports
  std::shared_ptr<TextData> m_outputText;
  bool m_processingInput = false;
};
