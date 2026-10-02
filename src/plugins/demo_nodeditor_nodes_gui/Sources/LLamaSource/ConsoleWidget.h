#pragma once

#include <QtWidgets/QWidget>

#include <QJsonArray>
#include <QJsonObject>

class ChatBaseWidget;

/**
 * @brief Chat UI of the Console node (REQ-SW-PL-051).
 *
 * Thin container around ChatBaseWidget, mirroring what ConsoleDataModel used
 * to build in its constructor: a QWidget with a 4px-margin QVBoxLayout holding
 * the chat, with the config panel hidden by default.
 */
class ConsoleWidget : public QWidget
{
    Q_OBJECT

public:
    explicit ConsoleWidget(QWidget *parent = nullptr);
    ~ConsoleWidget() override = default;

public slots:
    /// A response arrived on the model's input port.
    void addResponse(QString const& content, QJsonObject const& rawJson);

signals:
    void sendRequested(QString const& text, QJsonArray const& messages,
                       double temperature, int nPredict);
    void configChanged(QJsonObject const& config);

private:
    ChatBaseWidget* m_chatWidget = nullptr;
};
