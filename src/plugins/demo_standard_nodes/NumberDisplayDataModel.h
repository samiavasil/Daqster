#pragma once

#include <QtCore/QObject>
#include <QtWidgets/QLabel>

#include <QtNodes/NodeDelegateModel>

#include <iostream>

/// The model dictates the number of inputs and outputs for the Node.
/// In this example it has no logic.
class NumberDisplayDataModel : public QtNodes::NodeDelegateModel
{
    Q_OBJECT

public:
    NumberDisplayDataModel();

    virtual
    ~NumberDisplayDataModel() {}

public:

    QString
    caption() const override
    { return QStringLiteral("Result"); }

    bool
    captionVisible() const override
    { return false; }

    QString
    name() const override
    { return QStringLiteral("Result"); }

public:

    unsigned int
    nPorts(QtNodes::PortType portType) const override;

    QtNodes::NodeDataType
    dataType(QtNodes::PortType portType,
             QtNodes::PortIndex portIndex) const override;

    std::shared_ptr<QtNodes::NodeData>
    outData(QtNodes::PortIndex const port) override;

    void
    setInData(std::shared_ptr<QtNodes::NodeData> data, QtNodes::PortIndex const portIndex) override;

    QWidget *
    embeddedWidget() override { return _label; }

    QtNodes::NodeValidationState
    validationState() const override { return modelValidationState; }

private:

    QtNodes::NodeValidationState modelValidationState;
    QString modelValidationError = QStringLiteral("Missing or incorrect inputs");

    QLabel * _label;
};
