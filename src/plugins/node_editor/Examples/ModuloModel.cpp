#include "ModuloModel.h"


#include <QtGui/QDoubleValidator>
#include "NumericType.h"
#include <cmath>
template<typename ValueType>
ModuloModel<ValueType>::ModuloModel(){
    m_w = new QComboBox();
    m_w->addItem("edno");
    m_w->addItem("dwe");
    m_w->addItem("tri");

}

template<typename ValueType>
ModuloModel<ValueType>::~ModuloModel()
{
    if((!m_wembed) && m_w){
        m_w->deleteLater();
    }
}

template<typename ValueType>
QJsonObject
ModuloModel<ValueType>::
save() const
{
    QJsonObject modelJson;

    modelJson["name"] = name();

    return modelJson;
}

template<typename ValueType>
unsigned int
ModuloModel<ValueType>::
nPorts(PortType portType) const
{
    unsigned int result = 1;

    switch (portType)
    {
    case PortType::In:
        result = 2;
        break;

    case PortType::Out:
        result = 1;

    default:
        break;
    }

    return result;
}

template<typename ValueType>
NodeDataType
ModuloModel<ValueType>::
dataType(PortType, PortIndex) const
{
    return NumericType<ValueType>().type();
}

template<typename ValueType>
std::shared_ptr<NodeData>
ModuloModel<ValueType>::
outData(PortIndex)
{
    return _result;
}


static inline int mod(int a, int b){
    return (a%b);
}

static inline double mod(double a, double b){
    return fmod(a, b);
}

template<typename ValueType>

void
ModuloModel<ValueType>::
setInData(std::shared_ptr<NodeData> data, PortIndex portIndex)
{
    auto numberData =
            std::dynamic_pointer_cast<NumericType<ValueType>>(data);

    if (portIndex == 0)
    {
        _number1 = numberData;
    }
    else
    {
        _number2 = numberData;
    }

    {
        PortIndex const outPortIndex = 0;

        auto n1 = std::dynamic_pointer_cast<NumericType<ValueType>>(_number1.lock());
        auto n2 = std::dynamic_pointer_cast<NumericType<ValueType>>(_number2.lock());

        if (n2 && (n2->number() == 0))
        {
            modelValidationState = NodeValidationState::Error;
            modelValidationError = QStringLiteral("Division by zero error");
            _result.reset();
        }
        else if (n1 && n2)
        {
            modelValidationState = NodeValidationState::Valid;
            modelValidationError = QString();
            _result = std::make_shared<NumericType<ValueType>>(mod(n1->number(), n2->number()));
        }
        else
        {
            modelValidationState = NodeValidationState::Warning;
            modelValidationError = QStringLiteral("Missing or incorrect inputs");
            _result.reset();
        }

        Q_EMIT dataUpdated(outPortIndex);
    }
}

template<typename ValueType>
NodeValidationState
ModuloModel<ValueType>::
validationState() const
{
    return modelValidationState;
}

template<typename ValueType>
QString
ModuloModel<ValueType>::
validationMessage() const
{
    return modelValidationError;
}
