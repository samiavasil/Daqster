#pragma once

#include <QtCore/QObject>
#include <QtWidgets/QWidget>
#include <QtWidgets/QVBoxLayout>

#include <QtNodes/NodeDelegateModel>

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

#include <memory>

#include "NodeDataTypes/TextData.h"
#include "ChatBaseWidget.h"

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
    QWidget *embeddedWidget() override { return m_ui; }
    bool resizable() const override { return true; }

    QJsonObject save() const override;
    void load(QJsonObject const& p) override;

private Q_SLOTS:
    void onSendClicked(QString const& text, QJsonArray const& messages,
                       double temperature, int nPredict);

private:
    QWidget *m_ui;
    ChatBaseWidget *m_chatWidget;

    std::shared_ptr<TextData> m_outputText;
};
