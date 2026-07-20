
#pragma once

#include <QtNodes/NodeDelegateModel>

/// The class can potentially incapsulate any user data which
/// need to be transferred within the Node Editor graph
class IntegerData : public QtNodes::NodeData
{
public:

  IntegerData()
    : _number(0.0)
  {}

  IntegerData(int const number)
    : _number(number)
  {}

  QtNodes::NodeDataType type() const override
  {
    return QtNodes::NodeDataType {"integer",
                         "Integer"};
  }

  int number() const
  { return _number; }

  QString numberAsText() const
  { return QString::number(_number); }

private:

  int _number;
};

