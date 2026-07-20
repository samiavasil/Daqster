#pragma once

#include <QtCore/QObject>
#include <QtWidgets/QLineEdit>

#include <QtNodes/NodeDelegateModel>

#include <iostream>
#include <NumericType.h>

class NumberSourceDataUi;
/// The model dictates the number of inputs and outputs for the Node.
/// In this example it has no logic.
class NumberSourceDataModel
        : public QtNodes::NodeDelegateModel
{
    Q_OBJECT

public:
    NumberSourceDataModel();

    virtual
    ~NumberSourceDataModel();

public:

    QString
    caption() const override
    { return QStringLiteral("Number Source"); }

    bool
    captionVisible() const override
    { return false; }

    QString
    name() const override
    { return QStringLiteral("NumberSource"); }

public:

    QJsonObject
    save() const override;

    void
    load(QJsonObject const &p) override;

public:

    unsigned int
    nPorts(QtNodes::PortType portType) const override;

    QtNodes::NodeDataType
    dataType(QtNodes::PortType portType, QtNodes::PortIndex portIndex) const override;

    std::shared_ptr<QtNodes::NodeData>
    outData(QtNodes::PortIndex const port) override;

    void
    setInData(std::shared_ptr<QtNodes::NodeData> data, QtNodes::PortIndex const port) override;

    QWidget *
    embeddedWidget() override;


protected slots:
    void ChangeTime(int t);
private Q_SLOTS:

    void
    onTextEdited(QString const &string);

private:

    std::shared_ptr<NumericType<double>> _number;

    NumberSourceDataUi * m_ui;

    int m_time;
};
