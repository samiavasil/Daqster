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

#include "TextData.h"
#include "ChatBaseWidget.h"

using QtNodes::PortType;
using QtNodes::PortIndex;
using QtNodes::NodeData;
using QtNodes::NodeDataType;
using QtNodes::NodeDelegateModel;
using QtNodes::NodeValidationState;

class LLamaModelDataModel : public NodeDelegateModel {
  Q_OBJECT

public:
  LLamaModelDataModel();
  virtual ~LLamaModelDataModel();

  QString caption() const override { return QStringLiteral("LLaMA Model"); }
  bool captionVisible() const override { return true; }
  QString name() const override { return QStringLiteral("LLamaModel"); }

  unsigned int nPorts(PortType portType) const override;
  NodeDataType dataType(PortType portType, PortIndex portIndex) const override;
  std::shared_ptr<NodeData> outData(PortIndex const port) override;
  void setInData(std::shared_ptr<NodeData> data, PortIndex const portIndex) override;
  QWidget* embeddedWidget() override { return m_ui; }
  bool resizable() const override { return true; }

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
