#include "LLamaModelDataModel.h"

#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QJsonArray>
#include <QtCore/QUrl>
#include <QtCore/QFileInfo>
#include <QtCore/QUuid>
#include <QtCore/QDateTime>
#include <QtWidgets/QFileDialog>
#include <QtWidgets/QFormLayout>
#include <QtWidgets/QGroupBox>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QHBoxLayout>

using QtNodes::PortType;
using QtNodes::PortIndex;
using QtNodes::NodeData;
using QtNodes::NodeDataType;
using QtNodes::NodeDelegateModel;

LLamaModelDataModel::LLamaModelDataModel()
    : m_ui(nullptr), m_tabWidget(nullptr), m_chatWidget(nullptr), m_netManager(new QNetworkAccessManager(this)), m_connected(false), m_serverProcess(nullptr), m_outputText(std::make_shared<TextData>("")), m_processingInput(false) {
  buildUi();
}

LLamaModelDataModel::~LLamaModelDataModel() {
  if (m_serverProcess && m_serverProcess->state() != QProcess::NotRunning) {
    m_serverProcess->terminate();
    m_serverProcess->waitForFinished(3000);
  }
}

void LLamaModelDataModel::buildUi() {
  m_ui = new QWidget();
  auto* mainLayout = new QVBoxLayout(m_ui);
  mainLayout->setContentsMargins(4, 4, 4, 4);
  mainLayout->setSpacing(4);

  m_tabWidget = new QTabWidget();
  m_tabWidget->addTab(createServerTab(), "Сървър");
  m_tabWidget->addTab(createDebugTab(), "Дебаг");
  m_tabWidget->addTab(createChatTab(), "Чат");

  mainLayout->addWidget(m_tabWidget);
}

QWidget* LLamaModelDataModel::createServerTab() {
  auto* w = new QWidget();
  auto* layout = new QVBoxLayout(w);
  layout->setSpacing(8);

  auto* connGroup = new QGroupBox("Съществуващ сървър");
  auto* connLayout = new QFormLayout(connGroup);

  auto* hostPortLayout = new QHBoxLayout();
  m_hostEdit = new QLineEdit("127.0.0.1");
  m_portSpin = new QSpinBox();
  m_portSpin->setRange(1, 65535);
  m_portSpin->setValue(8080);

  hostPortLayout->addWidget(m_hostEdit);
  hostPortLayout->addWidget(m_portSpin);

  connLayout->addRow("Хост:", hostPortLayout);

  auto* connBtnLayout = new QHBoxLayout();
  m_connectBtn = new QPushButton("Свържи");
  m_statusLabel = new QLabel("Не е свързан");
  m_statusLabel->setStyleSheet("color: gray;");
  connBtnLayout->addWidget(m_connectBtn);
  connBtnLayout->addWidget(m_statusLabel);
  connBtnLayout->addStretch();
  connLayout->addRow("", connBtnLayout);

  layout->addWidget(connGroup);

  auto* localGroup = new QGroupBox("Локален сървър");
  auto* localLayout = new QFormLayout(localGroup);

  auto* exeLayout = new QHBoxLayout();
  m_exePath = new QLineEdit();
  m_exePath->setPlaceholderText("Път до llama.cpp (./server)");
  m_browseExeBtn = new QPushButton("...");
  m_browseExeBtn->setMaximumWidth(30);
  exeLayout->addWidget(m_exePath);
  exeLayout->addWidget(m_browseExeBtn);
  localLayout->addRow("Изпълним:", exeLayout);

  auto* modelLayout = new QHBoxLayout();
  m_modelPath = new QLineEdit();
  m_modelPath->setPlaceholderText("Път до модел (.gguf)");
  m_browseModelBtn = new QPushButton("...");
  m_browseModelBtn->setMaximumWidth(30);
  modelLayout->addWidget(m_modelPath);
  modelLayout->addWidget(m_browseModelBtn);
  localLayout->addRow("Модел:", modelLayout);

  m_ctxSizeSpin = new QSpinBox();
  m_ctxSizeSpin->setRange(128, 65536);
  m_ctxSizeSpin->setValue(2048);
  m_ctxSizeSpin->setSingleStep(512);
  localLayout->addRow("Контекст:", m_ctxSizeSpin);

  m_useGpuCheck = new QCheckBox("Използвай GPU (ако наличен)");
  m_useGpuCheck->setChecked(true);
  localLayout->addRow("", m_useGpuCheck);

  m_startBtn = new QPushButton("Стартирай сървър");
  m_startBtn->setStyleSheet("QPushButton { color: white; background-color: #4CAF50; }");
  localLayout->addRow("", m_startBtn);

  layout->addWidget(localGroup);
  layout->addStretch();

  connect(m_connectBtn, &QPushButton::clicked, this, &LLamaModelDataModel::onConnectClicked);
  connect(m_startBtn, &QPushButton::clicked, this, &LLamaModelDataModel::onStartServerClicked);
  connect(m_browseExeBtn, &QPushButton::clicked, this, &LLamaModelDataModel::onBrowseExe);
  connect(m_browseModelBtn, &QPushButton::clicked, this, &LLamaModelDataModel::onBrowseModel);

  return w;
}

QWidget* LLamaModelDataModel::createDebugTab() {
  auto* w = new QWidget();
  auto* layout = new QVBoxLayout(w);

  auto* pathLayout = new QHBoxLayout();
  m_debugPath = new QLineEdit("/completion");
  m_debugSendBtn = new QPushButton("Изпрати");
  pathLayout->addWidget(new QLabel("Endpoint:"));
  pathLayout->addWidget(m_debugPath);
  pathLayout->addWidget(m_debugSendBtn);

  m_debugBody = new QTextEdit();
  m_debugBody->setPlaceholderText("JSON body (напр. {\"prompt\":\"Hello\",\"n_predict\":128})");
  m_debugBody->setMaximumHeight(120);

  m_debugResponse = new QTextEdit();
  m_debugResponse->setReadOnly(true);
  m_debugResponse->setPlaceholderText("Отговор...");

  auto* splitter = new QSplitter(Qt::Vertical);
  splitter->addWidget(m_debugBody);
  splitter->addWidget(m_debugResponse);

  layout->addLayout(pathLayout);
  layout->addWidget(splitter);

  connect(m_debugSendBtn, &QPushButton::clicked, this, &LLamaModelDataModel::onDebugSendClicked);

  return w;
}

QWidget* LLamaModelDataModel::createChatTab() {
  m_chatWidget = new ChatBaseWidget();

  connect(m_chatWidget, &ChatBaseWidget::sendRequested,
          this, &LLamaModelDataModel::onLocalChatSend);

  return m_chatWidget;
}

unsigned int LLamaModelDataModel::nPorts(PortType portType) const {
  switch (portType) {
    case PortType::In:
      return 1;
    case PortType::Out:
      return 1;
    default:
      return 0;
  }
}

NodeDataType LLamaModelDataModel::dataType(PortType, PortIndex) const {
  return TextData().type();
}

std::shared_ptr<NodeData> LLamaModelDataModel::outData(PortIndex const) {
  return m_outputText;
}

// ---------------------------------------------------------------------------
// Local chat send (from Chat tab)
// ---------------------------------------------------------------------------
void LLamaModelDataModel::onLocalChatSend(QString const& text, QJsonArray const& messages,
                                           double temperature, int nPredict) {
  Q_UNUSED(text)

  sendToModel(messages, temperature, nPredict,
              [this](QString content) {
                if (content.isEmpty())
                  return;

                // Add response to chat widget (updates session + display)
                m_chatWidget->addResponse(content);
              });
}

// ---------------------------------------------------------------------------
// Stateless sendToModel
// ---------------------------------------------------------------------------
void LLamaModelDataModel::sendToModel(QJsonArray const& messages, double temperature, int nPredict,
                                      std::function<void(QString)> onResult) {
  if (!m_connected && !m_serverProcess) {
    if (onResult)
      onResult(QString());
    return;
  }

  if (!m_connected) {
    if (onResult)
      onResult(QString());
    return;
  }

  m_processingInput = true;

  QUrl url(m_baseUrl + "/v1/chat/completions");
  QNetworkRequest req(url);
  req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

  QJsonObject body;
  body["messages"] = messages;
  body["temperature"] = temperature;
  body["n_predict"] = nPredict;
  body["stream"] = false;

  QNetworkReply* reply = m_netManager->post(req, QJsonDocument(body).toJson(QJsonDocument::Compact));
  connect(reply, &QNetworkReply::finished, this, [this, reply, body, onResult]() {
    reply->deleteLater();
    m_processingInput = false;

    if (reply->error() == QNetworkReply::NoError) {
      QByteArray data = reply->readAll();
      QJsonDocument doc = QJsonDocument::fromJson(data);
      if (doc.isObject()) {
        QJsonObject obj = doc.object();
        QJsonArray choices = obj["choices"].toArray();
        if (!choices.isEmpty()) {
          QJsonObject firstChoice = choices.at(0).toObject();
          QJsonObject messageObj = firstChoice["message"].toObject();
          QString content = messageObj["content"].toString();

          if (!content.isEmpty()) {
            QJsonObject exchangeObj;
            exchangeObj["request_body"] = body;
            exchangeObj["response"] = obj;
            QString fullOutput = QString::fromUtf8(QJsonDocument(exchangeObj).toJson(QJsonDocument::Compact));
            m_outputText = std::make_shared<TextData>(fullOutput);
            Q_EMIT dataUpdated(0);

            // Добавяме exchange JSON в локалния JSON tree
            if (m_chatWidget != nullptr)
              m_chatWidget->appendJsonToTree(exchangeObj, false);

            if (onResult)
              onResult(content);
          }
        }
      }
    } else {
      if (onResult)
        onResult(QString());
    }
  });
}

// ---------------------------------------------------------------------------
// setInData - външна заявка от Console
// ---------------------------------------------------------------------------
void LLamaModelDataModel::setInData(std::shared_ptr<NodeData> data, PortIndex const) {
  if (m_processingInput)
    return;

  auto textData = std::dynamic_pointer_cast<TextData>(data);
  if (!textData || textData->text().isEmpty())
    return;

  QString raw = textData->text();

  QJsonDocument doc = QJsonDocument::fromJson(raw.toUtf8());
  if (!doc.isObject())
    return;

  QJsonObject obj = doc.object();

  QJsonArray messages = obj["messages"].toArray();
  double temperature = obj["temperature"].toDouble(0.3);
  int nPredict = obj["n_predict"].toInt(512);

  if (messages.isEmpty())
    return;

  // Stateless: не пипаме UI/сесии, само изпращаме
  sendToModel(messages, temperature, nPredict);
}

// ---------------------------------------------------------------------------
// Server connection
// ---------------------------------------------------------------------------
void LLamaModelDataModel::onConnectClicked() {
  if (m_connected) {
    m_connected = false;
    m_connectBtn->setText("Свържи");
    setStatus("Изключен", false);
    return;
  }

  QString host = m_hostEdit->text().trimmed();
  int port = m_portSpin->value();
  m_baseUrl = QString("http://%1:%2").arg(host).arg(port);

  m_connectBtn->setEnabled(false);
  setStatus("Свързване...", false);
  checkHealth();
}

void LLamaModelDataModel::checkHealth() {
  QUrl url(m_baseUrl + "/health");
  QNetworkRequest req(url);
  req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

  auto* reply = m_netManager->get(req);
  connect(reply, &QNetworkReply::finished, this, [this, reply]() {
    reply->deleteLater();
    if (reply->error() == QNetworkReply::NoError) {
      QByteArray data = reply->readAll();
      QJsonDocument doc = QJsonDocument::fromJson(data);
      if (doc.isObject()) {
        QJsonObject obj = doc.object();
        QString status = obj["status"].toString();
        if (status == "ok" || status == "running" || status == "healthy") {
          m_connected = true;
          m_connectBtn->setText("Изключи");
          setStatus("Свързан", true);
          m_connectBtn->setEnabled(true);
          return;
        }
      }
    }
    m_connected = false;
    m_connectBtn->setText("Свържи");
    setStatus("Грешка: " + reply->errorString(), false);
    m_connectBtn->setEnabled(true);
  });
}

void LLamaModelDataModel::onStartServerClicked() {
  if (m_serverProcess && m_serverProcess->state() != QProcess::NotRunning) {
    m_serverProcess->terminate();
    m_serverProcess->waitForFinished(3000);
    m_startBtn->setText("Стартирай сървър");
    m_startBtn->setStyleSheet("QPushButton { color: white; background-color: #4CAF50; }");
    setStatus("Локален сървър спрян", false);
    m_connected = false;
    m_connectBtn->setText("Свържи");
    return;
  }

  QString exe = m_exePath->text().trimmed();
  QString model = m_modelPath->text().trimmed();

  if (exe.isEmpty()) {
    setStatus("Няма избран изпълним файл", false);
    return;
  }
  if (model.isEmpty()) {
    setStatus("Няма избран модел", false);
    return;
  }

  m_serverProcess = new QProcess(this);
  QStringList args;
  args << "-m" << model
       << "--host" << m_hostEdit->text().trimmed()
       << "--port" << QString::number(m_portSpin->value())
       << "-c" << QString::number(m_ctxSizeSpin->value());

  if (m_useGpuCheck->isChecked()) {
    args << "-ngl" << "99";
  }

  connect(m_serverProcess, &QProcess::started, this, [this]() {
    m_startBtn->setText("Спри сървър");
    m_startBtn->setStyleSheet("QPushButton { color: white; background-color: #f44336; }");
    setStatus("Локален сървър стартира...", false);
  });

  connect(m_serverProcess, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
          this, [this](int exitCode, QProcess::ExitStatus) {
            m_startBtn->setText("Стартирай сървър");
            m_startBtn->setStyleSheet("QPushButton { color: white; background-color: #4CAF50; }");
            setStatus(QString("Сървър спрян (код: %1)").arg(exitCode), false);
            m_connected = false;
            m_connectBtn->setText("Свържи");
          });

  connect(m_serverProcess, &QProcess::readyReadStandardOutput, this, [this]() {
    QByteArray out = m_serverProcess->readAllStandardOutput();
    if (out.contains("running") || out.contains("start") || out.contains("listen")) {
      setStatus("Локален сървър работи", true);
      m_connected = true;
      m_connectBtn->setText("Изключи");

      QString host = m_hostEdit->text().trimmed();
      int port = m_portSpin->value();
      m_baseUrl = QString("http://%1:%2").arg(host).arg(port);
    }
  });

  connect(m_serverProcess, &QProcess::readyReadStandardError, this, [this]() {
    QByteArray err = m_serverProcess->readAllStandardError();
    if (err.contains("running") || err.contains("start") || err.contains("listen")) {
      setStatus("Локален сървър работи", true);
      m_connected = true;
      m_connectBtn->setText("Изключи");
    }
  });

  m_serverProcess->start(exe, args);
}

void LLamaModelDataModel::onBrowseExe() {
  QString path = QFileDialog::getOpenFileName(m_ui, "Изберете llama.cpp сървър");
  if (!path.isEmpty())
    m_exePath->setText(path);
}

void LLamaModelDataModel::onBrowseModel() {
  QString path = QFileDialog::getOpenFileName(m_ui, "Изберете модел (.gguf)", QString(), "GGUF файлове (*.gguf)");
  if (!path.isEmpty())
    m_modelPath->setText(path);
}

void LLamaModelDataModel::onDebugSendClicked() {
  if (!m_connected) {
    m_debugResponse->setPlainText("Няма свързан сървър.");
    return;
  }

  QString endpoint = m_debugPath->text().trimmed();
  if (!endpoint.startsWith("/"))
    endpoint = "/" + endpoint;

  QUrl url(m_baseUrl + endpoint);
  QNetworkRequest req(url);
  req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

  QString bodyText = m_debugBody->toPlainText().trimmed();
  QByteArray bodyData;
  if (!bodyText.isEmpty()) {
    bodyData = bodyText.toUtf8();
  }

  QNetworkReply* reply;
  if (bodyData.isEmpty()) {
    reply = m_netManager->get(req);
  } else {
    reply = m_netManager->post(req, bodyData);
  }

  connect(reply, &QNetworkReply::finished, this, [this, reply]() {
    reply->deleteLater();

    QString result;
    if (reply->error() == QNetworkReply::NoError) {
      QByteArray data = reply->readAll();
      QJsonDocument doc = QJsonDocument::fromJson(data);
      if (doc.isObject()) {
        result = QString::fromUtf8(QJsonDocument(doc).toJson(QJsonDocument::Indented));
      } else {
        result = QString::fromUtf8(data);
      }
    } else {
      result = "Грешка: " + reply->errorString();
    }

    m_debugResponse->setPlainText(result);
  });
}

void LLamaModelDataModel::setStatus(QString const& status, bool connected) {
  m_statusLabel->setText(status);
  if (connected) {
    m_statusLabel->setStyleSheet("color: green;");
  } else {
    m_statusLabel->setStyleSheet("color: gray;");
  }
}

// ---------------------------------------------------------------------------
// save / load
// ---------------------------------------------------------------------------
QJsonObject LLamaModelDataModel::save() const {
  QJsonObject obj = NodeDelegateModel::save();
  obj["host"] = m_hostEdit->text();
  obj["port"] = m_portSpin->value();
  obj["exePath"] = m_exePath->text();
  obj["modelPath"] = m_modelPath->text();
  obj["ctxSize"] = m_ctxSizeSpin->value();
  obj["useGpu"] = m_useGpuCheck->isChecked();

  if (m_chatWidget != nullptr) {
    QJsonObject chatConfig = m_chatWidget->saveConfig();
    // Merge chat config into root
    for (auto it = chatConfig.begin(); it != chatConfig.end(); ++it)
      obj[it.key()] = it.value();
  }

  return obj;
}

void LLamaModelDataModel::load(QJsonObject const& p) {
  if (p.contains("host")) m_hostEdit->setText(p["host"].toString());
  if (p.contains("port")) m_portSpin->setValue(p["port"].toInt());
  if (p.contains("exePath")) m_exePath->setText(p["exePath"].toString());
  if (p.contains("modelPath")) m_modelPath->setText(p["modelPath"].toString());
  if (p.contains("ctxSize")) m_ctxSizeSpin->setValue(p["ctxSize"].toInt());
  if (p.contains("useGpu")) m_useGpuCheck->setChecked(p["useGpu"].toBool());

  if (m_chatWidget != nullptr)
    m_chatWidget->loadConfig(p);
}
