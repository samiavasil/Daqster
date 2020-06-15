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

  ComplexType(_T* data)
  {
      _data = std::shared_ptr<_T>(data);
  }

  NodeDataType type() const override
  {
    return NodeDataType {typeid(_T).name(),
                         typeid(_T).name()};
  }

  _T& data() const
  { return *_data; }

private:

  std::shared_ptr<_T> _data;
};

#endif // COMPLEXTYPE_H
