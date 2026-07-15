#pragma once

#include <QtNodes/NodeDelegateModel>

/// The class can potentially incapsulate any user data which
/// need to be transferred within the Node Editor graph
class DecimalData : public QtNodes::NodeData
{
public:

  DecimalData()
    : _number(0.0)
  {}

  DecimalData(double const number)
    : _number(number)
  {}

  QtNodes::NodeDataType type() const override
  {
    return QtNodes::NodeDataType {"decimal",
                         "Decimal"};
  }

  double number() const
  { return _number; }

  QString numberAsText() const
  { return QString::number(_number, 'f'); }

private:

  double _number;
};
