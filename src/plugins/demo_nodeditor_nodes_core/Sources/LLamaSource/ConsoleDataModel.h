#pragma once

#include <QtCore/QObject>
#include <QtNodes/NodeDelegateModel>

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

#include <memory>

#include "NodeDataTypes/TextData.h"

class ConsoleDataModel : public QtNodes::NodeDelegateModel
{
    Q_OBJECT

public:
    ConsoleDataModel();
    virtual ~ConsoleDataModel() = default;

    QString caption() const override { return QStringLiteral("Конзола"); }
    bool captionVisible() const override { return true; }
    QString name() const override { return QStringLiteral("Console"); }

    unsigned int nPorts(QtNodes::PortType portType) const override;
    QtNodes::NodeDataType dataType(QtNodes::PortType portType, QtNodes::PortIndex portIndex) const override;
    std::shared_ptr<QtNodes::NodeData> outData(QtNodes::PortIndex const port) override;
    void setInData(std::shared_ptr<QtNodes::NodeData> data, QtNodes::PortIndex const portIndex) override;

    /// Core model has no QtWidgets dependency — ChatBaseWidget is created by
    /// the GUI plugin and wired through NodeWidgetFactory (REQ-SW-PL-051).
    QWidget *embeddedWidget() override { return nullptr; }
    bool resizable() const override { return true; }

    /// The node BODY (boundary, caption, ports) does not depend on data —
    /// widget content self-repaints via Qt. Opts out of the body repaint.
    bool dataArrivalChangesWidget() const override { return false; }

    QJsonObject save() const override;
    void load(QJsonObject const& p) override;

    /// Persisted chat state, for the GUI plugin to hydrate ChatBaseWidget.
    QJsonObject chatConfig() const { return m_chatConfig; }

public slots:
    /// Send request from ChatBaseWidget (called by NodeWidgetFactory).
    void onSendClicked(QString const& text, QJsonArray const& messages,
                       double temperature, int nPredict);

    /// The widget's persisted chat state (sessions / prompt / temperature /
    /// nPredict) — mirrored here so the model can serialize without QtWidgets.
    void onChatConfigChanged(QJsonObject const& config);

signals:
    /// Model -> widget: a response arrived on the input port.
    void responseReceived(QString const& content, QJsonObject const& rawJson);

private:
    /// Last chat config reported by the widget, merged into save().
    QJsonObject m_chatConfig;

    std::shared_ptr<TextData> m_outputText;
};
