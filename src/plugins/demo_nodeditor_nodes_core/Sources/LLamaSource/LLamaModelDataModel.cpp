#include "LLamaModelDataModel.h"

#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QJsonArray>
#include <QtCore/QUrl>

using QtNodes::PortType;
using QtNodes::PortIndex;
using QtNodes::NodeData;
using QtNodes::NodeDataType;
using QtNodes::NodeDelegateModel;

namespace {
const char* kServerRunningStyle = "QPushButton { color: white; background-color: #4CAF50; }";
const char* kServerStoppedStyle = "QPushButton { color: white; background-color: #f44336; }";
} // namespace

LLamaModelDataModel::LLamaModelDataModel()
    : m_netManager(new QNetworkAccessManager(this))
    , m_outputText(std::make_shared<TextData>(""))
{
}

LLamaModelDataModel::~LLamaModelDataModel() {
  // Single shutdown path: stop() terminates the server process (REQ-SW-PL-050).
  stop();
}

void LLamaModelDataModel::stop() {
  // Idempotent: terminate()/waitForFinished() on a NotRunning process is a
  // no-op.
  if (m_serverProcess && m_serverProcess->state() != QProcess::NotRunning) {
    m_serverProcess->terminate();
    m_serverProcess->waitForFinished(3000);
  }
}

/// Start the local server process programmatically (runtime autoStart, REQ-SW-PL-048).
/// Delegates to the same logic as onStartServerClicked() when not running.
void LLamaModelDataModel::start() {
  if (m_serverProcess && m_serverProcess->state() != QProcess::NotRunning)
    return;
  onStartServerClicked();
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

                // Add response to the chat widget (updates session + display)
                Q_EMIT chatResponseReceived(content, QJsonObject());
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

            // Add the exchange JSON to the local JSON tree
            Q_EMIT chatResponseReceived(content, exchangeObj);

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
// setInData - external request from Console
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

  // Stateless: we don't touch the UI/sessions, we only send
  sendToModel(messages, temperature, nPredict);
}

// ---------------------------------------------------------------------------
// Server connection
// ---------------------------------------------------------------------------
void LLamaModelDataModel::onConnectClicked() {
  if (m_connected) {
    m_connected = false;
    Q_EMIT connectButtonChanged(QStringLiteral("Свържи"), true);
    setStatus(QStringLiteral("Изключен"), false);
    return;
  }

  m_baseUrl = QString("http://%1:%2").arg(m_host.trimmed()).arg(m_port);

  Q_EMIT connectButtonChanged(m_connected ? QStringLiteral("Изключи") : QStringLiteral("Свържи"), false);
  setStatus(QStringLiteral("Свързване..."), false);
  checkHealth();
}

void LLamaModelDataModel::checkHealth() {
  m_healthPending = true;

  QUrl url(m_baseUrl + "/health");
  QNetworkRequest req(url);
  req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

  QNetworkReply* reply = m_netManager->get(req);
  connect(reply, &QNetworkReply::finished, this, [this, reply]() {
    reply->deleteLater();

    bool healthy = false;
    QString error;
    if (reply->error() == QNetworkReply::NoError) {
      QByteArray data = reply->readAll();
      QJsonDocument doc = QJsonDocument::fromJson(data);
      if (doc.isObject()) {
        QString status = doc.object()["status"].toString();
        healthy = (status == "ok" || status == "running" || status == "healthy");
      }
    } else {
      error = reply->errorString();
    }

    onHealthReply(healthy, error);
  });
}

void LLamaModelDataModel::onHealthReply(bool healthy, QString const& error) {
  m_healthPending = false;

  if (healthy) {
    m_connected = true;
    Q_EMIT connectButtonChanged(QStringLiteral("Изключи"), true);
    setStatus(QStringLiteral("Свързан"), true);
    return;
  }

  m_connected = false;
  Q_EMIT connectButtonChanged(QStringLiteral("Свържи"), true);
  setStatus(QStringLiteral("Грешка: ") + error, false);
}

void LLamaModelDataModel::onStartServerClicked() {
  if (m_serverProcess && m_serverProcess->state() != QProcess::NotRunning) {
    m_serverProcess->terminate();
    m_serverProcess->waitForFinished(3000);
    Q_EMIT startButtonChanged(QStringLiteral("Стартирай сървър"), QString::fromLatin1(kServerRunningStyle));
    setStatus(QStringLiteral("Локален сървър спрян"), false);
    m_connected = false;
    Q_EMIT connectButtonChanged(QStringLiteral("Свържи"), true);
    return;
  }

  QString exe = m_exePath.trimmed();
  QString model = m_modelPath.trimmed();

  if (exe.isEmpty()) {
    setStatus(QStringLiteral("Няма избран изпълним файл"), false);
    return;
  }
  if (model.isEmpty()) {
    setStatus(QStringLiteral("Няма избран модел"), false);
    return;
  }

  m_serverProcess = new QProcess(this);
  QStringList args;
  args << "-m" << model
       << "--host" << m_host.trimmed()
       << "--port" << QString::number(m_port)
       << "-c" << QString::number(m_ctxSize);

  if (m_useGpu) {
    args << "-ngl" << "99";
  }

  connect(m_serverProcess, &QProcess::started,
          this, &LLamaModelDataModel::onProcessStarted);
  connect(m_serverProcess, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
          this, &LLamaModelDataModel::onProcessFinished);
  connect(m_serverProcess, &QProcess::readyReadStandardOutput,
          this, &LLamaModelDataModel::onProcessStdout);
  connect(m_serverProcess, &QProcess::readyReadStandardError,
          this, &LLamaModelDataModel::onProcessStderr);

  m_serverProcess->start(exe, args);
}

void LLamaModelDataModel::onProcessStarted() {
  Q_EMIT startButtonChanged(QStringLiteral("Спри сървър"), QString::fromLatin1(kServerStoppedStyle));
  setStatus(QStringLiteral("Локален сървър стартира..."), false);
}

void LLamaModelDataModel::onProcessFinished(int exitCode, QProcess::ExitStatus) {
  Q_EMIT startButtonChanged(QStringLiteral("Стартирай сървър"), QString::fromLatin1(kServerRunningStyle));
  setStatus(QString("Сървър спрян (код: %1)").arg(exitCode), false);
  m_connected = false;
  Q_EMIT connectButtonChanged(QStringLiteral("Свържи"), true);
}

void LLamaModelDataModel::onProcessStdout() {
  QByteArray out = m_serverProcess->readAllStandardOutput();
  if (out.contains("running") || out.contains("start") || out.contains("listen")) {
    setStatus(QStringLiteral("Локален сървър работи"), true);
    m_connected = true;
    Q_EMIT connectButtonChanged(QStringLiteral("Изключи"), true);
    m_baseUrl = QString("http://%1:%2").arg(m_host.trimmed()).arg(m_port);
  }
}

void LLamaModelDataModel::onProcessStderr() {
  QByteArray err = m_serverProcess->readAllStandardError();
  if (err.contains("running") || err.contains("start") || err.contains("listen")) {
    setStatus(QStringLiteral("Локален сървър работи"), true);
    m_connected = true;
    Q_EMIT connectButtonChanged(QStringLiteral("Изключи"), true);
  }
}

void LLamaModelDataModel::onExePathSelected(QString const& path) {
  m_exePath = path;
}

void LLamaModelDataModel::onModelPathSelected(QString const& path) {
  m_modelPath = path;
}

void LLamaModelDataModel::onDebugPathChanged(QString const& path) {
  m_debugPath = path;
}

void LLamaModelDataModel::onDebugBodyChanged(QString const& body) {
  m_debugBody = body;
}

void LLamaModelDataModel::onChatConfigChanged(QJsonObject const& config) {
  m_chatConfig = config;
}

void LLamaModelDataModel::onDebugSendClicked() {
  if (!m_connected) {
    Q_EMIT debugResponseReceived(QStringLiteral("Няма свързан сървър."));
    return;
  }

  QString endpoint = m_debugPath.trimmed();
  if (!endpoint.startsWith("/"))
    endpoint = "/" + endpoint;

  QUrl url(m_baseUrl + endpoint);
  QNetworkRequest req(url);
  req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

  QString bodyText = m_debugBody.trimmed();
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
      result = QStringLiteral("Грешка: ") + reply->errorString();
    }

    Q_EMIT debugResponseReceived(result);
  });
}

void LLamaModelDataModel::setStatus(QString const& status, bool connected) {
  Q_EMIT statusChanged(status, connected);
}

// ---------------------------------------------------------------------------
// save / load
// ---------------------------------------------------------------------------
QJsonObject LLamaModelDataModel::save() const {
  QJsonObject obj = NodeDelegateModel::save();
  obj["host"] = m_host;
  obj["port"] = m_port;
  obj["exePath"] = m_exePath;
  obj["modelPath"] = m_modelPath;
  obj["ctxSize"] = m_ctxSize;
  obj["useGpu"] = m_useGpu;

  // Merge chat config into root
  for (auto it = m_chatConfig.begin(); it != m_chatConfig.end(); ++it)
    obj[it.key()] = it.value();

  return obj;
}

void LLamaModelDataModel::load(QJsonObject const& p) {
  if (p.contains("host")) m_host = p["host"].toString();
  if (p.contains("port")) m_port = p["port"].toInt();
  if (p.contains("exePath")) m_exePath = p["exePath"].toString();
  if (p.contains("modelPath")) m_modelPath = p["modelPath"].toString();
  if (p.contains("ctxSize")) m_ctxSize = p["ctxSize"].toInt();
  if (p.contains("useGpu")) m_useGpu = p["useGpu"].toBool();

  m_chatConfig = p;
}
