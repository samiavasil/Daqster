#pragma once

#include <QtCore/QObject>
#include <QtCore/QProcess>
#include <QtNetwork/QNetworkAccessManager>
#include <QtNetwork/QNetworkReply>
#include <QtWidgets/QWidget>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QSpinBox>
#include <QtWidgets/QDoubleSpinBox>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QLabel>
#include <QtWidgets/QTextEdit>
#include <QtWidgets/QPlainTextEdit>
#include <QtWidgets/QTabWidget>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QCheckBox>
#include <QJsonArray>

#include <QtNodes/NodeDelegateModel>

#include <memory>
#include <functional>

#include "NodeDataTypes/TextData.h"
#include "ChatBaseWidget.h"

class LLamaModelDataModel : public QtNodes::NodeDelegateModel {
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
  QWidget* embeddedWidget() override { return m_ui; }
  bool resizable() const override { return true; }

  /// The node BODY (boundary, caption, ports) does not depend on data —
  /// widget content self-repaints via Qt. Opts out of the body repaint.
  bool dataArrivalChangesWidget() const override { return false; }

  QJsonObject save() const override;
  void load(QJsonObject const& p) override;

private Q_SLOTS:
  void onConnectClicked();
  void onStartServerClicked();
  void onBrowseExe();
  void onBrowseModel();
  void onDebugSendClicked();
  void onLocalChatSend(QString const& text, QJsonArray const& messages,
                       double temperature, int nPredict);

private:
  void buildUi();
  QWidget* createServerTab();
  QWidget* createDebugTab();
  QWidget* createChatTab();
  void checkHealth();

  // Stateless send - accepts messages[] array directly
  void sendToModel(QJsonArray const& messages, double temperature, int nPredict,
                   std::function<void(QString)> onResult = nullptr);

  void setStatus(QString const& status, bool connected);

  QWidget* m_ui;
  QTabWidget* m_tabWidget;

  // Server tab
  QLineEdit* m_hostEdit;
  QSpinBox* m_portSpin;
  QPushButton* m_connectBtn;
  QLabel* m_statusLabel;
  QLineEdit* m_exePath;
  QLineEdit* m_modelPath;
  QSpinBox* m_ctxSizeSpin;
  QCheckBox* m_useGpuCheck;
  QPushButton* m_startBtn;
  QPushButton* m_browseExeBtn;
  QPushButton* m_browseModelBtn;

  // Debug tab
  QLineEdit* m_debugPath;
  QTextEdit* m_debugBody;
  QPushButton* m_debugSendBtn;
  QTextEdit* m_debugResponse;

  // Chat tab
  ChatBaseWidget* m_chatWidget;

  // Networking
  QNetworkAccessManager* m_netManager;
  QString m_baseUrl;
  bool m_connected;

  // Local server
  QProcess* m_serverProcess;

  // Data ports
  std::shared_ptr<TextData> m_outputText;
  bool m_processingInput;
};
