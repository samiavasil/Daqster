#ifndef NUMERICXTYPE_H
#define NUMERICXTYPE_H

#pragma once

#include <nodes/NodeDataModel>

using QtNodes::NodeDataType;
using QtNodes::NodeData;

/// The class can potentially incapsulate any user data which
/// need to be transferred within the Node Editor graph
template<typename _T>
class NumericType : public NodeData
{
public:

  NumericType()
  {}

  NumericType(_T const number)
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
  { return QString::number(_number, 'f'); }

private:

  _T _number;
};

#endif // COMPLEXTYPE_H
