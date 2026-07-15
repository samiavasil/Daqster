#ifndef TESTNODEMODEL_H
#define TESTNODEMODEL_H


#pragma once

#include <QtCore/QObject>
#include <QtWidgets/QLineEdit>

#include <QtNodes/NodeDelegateModel>

#include <iostream>
#include <QComboBox>

class IntegerData;
#include "NumericType.h"

template<typename ValueType>
class ModuloModel
        : public QtNodes::NodeDelegateModel
{
public:

    ModuloModel();
    virtual
    ~ModuloModel();

public:

    QString
    caption() const override
    { return QString("Modulo %1").arg(typeid(ValueType).name()); }

    bool
    captionVisible() const override
    { return true; }

    bool
    portCaptionVisible(QtNodes::PortType, QtNodes::PortIndex ) const override
    { return true; }

    QString
    portCaption(QtNodes::PortType portType, QtNodes::PortIndex portIndex) const override
    {
        switch (portType)
        {
        case QtNodes::PortType::In:
            if (portIndex == 0)
                return QStringLiteral("Dividend");
            else if (portIndex == 1)
                return QStringLiteral("Divisor");

            break;

        case QtNodes::PortType::Out:
            return QStringLiteral("Result");

        default:
            break;
        }
        return QString();
    }

    QString
    name() const override
    { return QString("Modulo %1").arg(typeid(ValueType).name()); }

public:

    QJsonObject
    save() const override;

public:

    unsigned int
    nPorts(QtNodes::PortType portType) const override;

    QtNodes::NodeDataType
    dataType(QtNodes::PortType portType, QtNodes::PortIndex portIndex) const override;

    std::shared_ptr<QtNodes::NodeData>
    outData(QtNodes::PortIndex const port) override;

    void
    setInData(std::shared_ptr<QtNodes::NodeData> nodeData, QtNodes::PortIndex const portIndex) override;

    QWidget *
    embeddedWidget() override {
        return m_w;
    }

    QtNodes::NodeValidationState
    validationState() const override { return modelValidationState; }

private:

    std::weak_ptr<NumericType<ValueType>> _number1;
    std::weak_ptr<NumericType<ValueType>> _number2;

    std::shared_ptr<NumericType<ValueType>> _result;


    QtNodes::NodeValidationState modelValidationState;
    QString modelValidationError = QString("Missing or incorrect inputs");
    QComboBox* m_w;
};

template class ModuloModel<int>;
template class ModuloModel<double>;



#endif // TESTNODEMODEL_H
