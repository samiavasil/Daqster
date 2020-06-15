#pragma once

#include <QtCore/QObject>
#include <QtWidgets/QLineEdit>

#include <nodes/NodeDataModel>

#include <iostream>
#include<NumericType.h>

//class DecimalData;

using QtNodes::PortType;
using QtNodes::PortIndex;
using QtNodes::NodeData;
using QtNodes::NodeDataType;
using QtNodes::NodeDataModel;
using QtNodes::NodeValidationState;

class NumberSourceDataUi;
/// The model dictates the number of inputs and outputs for the Node.
/// In this example it has no logic.
class NumberSourceDataModel
        : public NodeDataModel
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
    restore(QJsonObject const &p) override;

public:

    unsigned int
    nPorts(PortType portType) const override;

    NodeDataType
    dataType(PortType portType, PortIndex portIndex) const override;

    std::shared_ptr<NodeData>
    outData(PortIndex port) override;

    void
    setInData(std::shared_ptr<NodeData> data, PortIndex port) override;

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
