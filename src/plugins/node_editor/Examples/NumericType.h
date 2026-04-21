#ifndef NUMERICXTYPE_H
#define NUMERICXTYPE_H

#pragma once

#include <nodes/NodeDataModel>

using QtNodes::NodeDataType;
using QtNodes::NodeData;

/// The class can potentially incapsulate any user data which
/// need to be transferred within the Node Editor graph
template<typename ValueType>
class NumericType : public NodeData
{
public:

  NumericType()
  {}

  NumericType(ValueType const number)
    : _number(number)
  {}

  NodeDataType type() const override
  {
    return NodeDataType {typeid(ValueType).name(),
                         typeid(ValueType).name()};
  }

  ValueType number() const
  { return _number; }

  QString numberAsText() const
  { return QString::number(_number, 'f'); }

private:

  ValueType _number;
};

#endif // COMPLEXTYPE_H
