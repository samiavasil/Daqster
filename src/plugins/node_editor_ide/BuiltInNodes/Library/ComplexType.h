#ifndef COMPLEXTYPE_H
#define COMPLEXTYPE_H

#pragma once

#include <QtNodes/NodeDelegateModel>

/// The class can potentially incapsulate any user data which
/// need to be transferred within the Node Editor graph
template<typename ValueType>
class ComplexType : public QtNodes::NodeData
{
public:

  ComplexType(ValueType* data)
  {
      _data = std::shared_ptr<ValueType>(data);
  }

  QtNodes::NodeDataType type() const override
  {
    return QtNodes::NodeDataType {typeid(ValueType).name(),
                         typeid(ValueType).name()};
  }

  ValueType& data() const
  { return *_data; }

private:

  std::shared_ptr<ValueType> _data;
};

#endif // COMPLEXTYPE_H
