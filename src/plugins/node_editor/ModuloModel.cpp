#include "ModuloModel.h"


#include <QtGui/QDoubleValidator>
#include "IntegerData.h"
#include "ComplexType.h"
#include <math.h>
template<typename _Tp>
ModuloModel< _Tp>::ModuloModel(){
    m_w = new QComboBox();
    m_w->addItem("edno");
    m_w->addItem("dwe");
    m_w->addItem("tri");

}

template<typename _Tp>
ModuloModel< _Tp>::~ModuloModel()
{
    if((!m_wembed) && m_w){
        m_w->deleteLater();
    }
}

template<typename _Tp>
QJsonObject
ModuloModel< _Tp>::
save() const
{
  QJsonObject modelJson;

  modelJson["name"] = name();

  return modelJson;
}

template<typename _Tp>
unsigned int
ModuloModel< _Tp>::
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

template<typename _Tp>
NodeDataType
ModuloModel< _Tp>::
dataType(PortType, PortIndex) const
{
  return ComplexType<_Tp>().type();
}

template<typename _Tp>
std::shared_ptr<NodeData>
ModuloModel< _Tp>::
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

template<typename _Tp>

void
ModuloModel< _Tp>::
setInData(std::shared_ptr<NodeData> data, PortIndex portIndex)
{
  auto numberData =
    std::dynamic_pointer_cast<  ComplexType<_Tp>>(data);

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

    auto n1 = std::dynamic_pointer_cast< ComplexType<_Tp>>(_number1.lock());
    auto n2 = std::dynamic_pointer_cast< ComplexType<_Tp>>(_number2.lock());

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
      _result = std::make_shared< ComplexType<_Tp>>(mod(n1->number(), n2->number()));
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

template<typename _Tp>
NodeValidationState
ModuloModel< _Tp>::
validationState() const
{
  return modelValidationState;
}

template<typename _Tp>
QString
ModuloModel< _Tp>::
validationMessage() const
{
  return modelValidationError;
}
