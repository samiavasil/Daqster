#ifndef TESTNODEMODEL_H
#define TESTNODEMODEL_H


#pragma once

#include <QtCore/QObject>
#include <QtWidgets/QLineEdit>

#include <nodes/NodeDataModel>

#include <iostream>
#include<QComboBox>

using QtNodes::PortType;
using QtNodes::PortIndex;
using QtNodes::NodeData;
using QtNodes::NodeDataType;
using QtNodes::NodeDataModel;
using QtNodes::NodeValidationState;

class IntegerData;
#include "NumericType.h"

template<typename _Tp>
class ModuloModel
  : public NodeDataModel
{

public:

  ModuloModel();
  virtual
  ~ModuloModel();

public:

  QString
  caption() const override
  { return QString("Modulo %1").arg(typeid(_Tp).name()); }

  bool
  captionVisible() const override
  { return true; }

  bool
  portCaptionVisible(PortType, PortIndex ) const override
  { return true; }

  QString
  portCaption(PortType portType, PortIndex portIndex) const override
  {
    switch (portType)
    {
      case PortType::In:
        if (portIndex == 0)
          return QStringLiteral("Dividend");
        else if (portIndex == 1)
          return QStringLiteral("Divisor");

        break;

      case PortType::Out:
        return QStringLiteral("Result");

      default:
        break;
    }
    return QString();
  }

  QString
  name() const override
  { return QString("Modulo %1").arg(typeid(_Tp).name()); }

public:

  QJsonObject
  save() const override;

public:

  unsigned int
  nPorts(PortType portType) const override;

  NodeDataType
  dataType(PortType portType, PortIndex portIndex) const override;

  std::shared_ptr<NodeData>
  outData(PortIndex port) override;

  void
  setInData(std::shared_ptr<NodeData>, int) override;

  QWidget *
  embeddedWidget() override {
      return m_w;
  }

  NodeValidationState
  validationState() const override;

  QString
  validationMessage() const override;

private:

  std::weak_ptr<NumericType<_Tp>> _number1;
  std::weak_ptr<NumericType<_Tp>> _number2;

  std::shared_ptr<NumericType<_Tp>> _result;


  NodeValidationState modelValidationState = NodeValidationState::Warning;
  QString modelValidationError = QStringLiteral("Missing or incorrect inputs");
  QComboBox* m_w;
};

template class ModuloModel<int>;
template class ModuloModel<double>;



#endif // TESTNODEMODEL_H
