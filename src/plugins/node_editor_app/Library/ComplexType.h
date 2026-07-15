#ifndef COMPLEXTYPE_H
#define COMPLEXTYPE_H

#pragma once

#include <QtNodes/NodeDelegateModel>

using QtNodes::NodeDataType;
using QtNodes::NodeData;

/// The class can potentially incapsulate any user data which
/// need to be transferred within the Node Editor graph
template<typename ValueType>
class ComplexType : public NodeData
{
public:

  ComplexType(ValueType* data)
  {
      _data = std::shared_ptr<ValueType>(data);
  }

  NodeDataType type() const override
  {
    return NodeDataType {typeid(ValueType).name(),
                         typeid(ValueType).name()};
  }

  ValueType& data() const
  { return *_data; }

private:

  std::shared_ptr<ValueType> _data;
};

#endif // COMPLEXTYPE_H
