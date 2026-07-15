#pragma once

#include <QtCore/QObject>
#include <QtWidgets/QLabel>

#include <QtNodes/NodeDelegateModel>

#include <iostream>

using QtNodes::PortType;
using QtNodes::PortIndex;
using QtNodes::NodeData;
using QtNodes::NodeDataType;
using QtNodes::NodeDelegateModel;
using QtNodes::NodeValidationState;

/// The model dictates the number of inputs and outputs for the Node.
/// In this example it has no logic.
class NumberDisplayDataModel : public NodeDelegateModel
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
    nPorts(PortType portType) const override;

    NodeDataType
    dataType(PortType portType,
             PortIndex portIndex) const override;

    std::shared_ptr<NodeData>
    outData(PortIndex const port) override;

    void
    setInData(std::shared_ptr<NodeData> data, PortIndex const portIndex) override;

    QWidget *
    embeddedWidget() override { return _label; }

    NodeValidationState
    validationState() const override { return modelValidationState; }

private:

    NodeValidationState modelValidationState;
    QString modelValidationError = QStringLiteral("Missing or incorrect inputs");

    QLabel * _label;
};
