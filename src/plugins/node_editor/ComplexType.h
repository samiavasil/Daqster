#ifndef COMPLEXTYPE_H
#define COMPLEXTYPE_H

#pragma once

#include <nodes/NodeDataModel>

using QtNodes::NodeDataType;
using QtNodes::NodeData;

/// The class can potentially incapsulate any user data which
/// need to be transferred within the Node Editor graph
template<typename _T>
class ComplexType : public NodeData
{
public:

  ComplexType()
    : _number(0.0)
  {}

  ComplexType(int const number)
    : _number(number)
  {}

  NodeDataType type() const override
  {
    return NodeDataType {typeid(_T).name(),
                         typeid(_T).name()};
  }

  _T number() const
  { return _number; }

  QString numberAsText() const
  { return QString::number(_number); }

private:

  _T _number;
};

#endif // COMPLEXTYPE_H
