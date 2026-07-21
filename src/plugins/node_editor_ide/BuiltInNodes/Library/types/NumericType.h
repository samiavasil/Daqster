#ifndef NUMERICXTYPE_H
#define NUMERICXTYPE_H

#pragma once

#include <QtNodes/NodeDelegateModel>

/// The class can potentially incapsulate any user data which
/// need to be transferred within the Node Editor graph
template<typename ValueType>
class NumericType : public QtNodes::NodeData
{
public:

  NumericType()
  {}

  NumericType(ValueType const number)
    : _number(number)
  {}

  QtNodes::NodeDataType type() const override
  {
    return QtNodes::NodeDataType {typeid(ValueType).name(),
                         typeid(ValueType).name()};
  }

  ValueType number() const
  { return _number; }

  QString numberAsText() const
  { return QString::number(static_cast<double>(_number), 'f', 0); }

private:

  ValueType _number;
};

#endif // COMPLEXTYPE_H
