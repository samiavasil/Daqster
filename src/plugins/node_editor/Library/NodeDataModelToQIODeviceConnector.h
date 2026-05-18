#ifndef NODEDATAMODELTOQIODEVICECONNECTOR_H
#define NODEDATAMODELTOQIODEVICECONNECTOR_H

#include <memory>
#include <QtNodes/NodeData>
#include <QtNodes/NodeDelegateModel>

using QtNodes::NodeDelegateModel;
using QtNodes::NodeDataType;
using QtNodes::NodeData;

class QIODevice;

class NodeDataModelToQIODeviceConnector :   public NodeData
{
public:
    NodeDataModelToQIODeviceConnector( NodeDelegateModel* model);
    virtual ~NodeDataModelToQIODeviceConnector() = default;
    virtual void ConnectModels(NodeDelegateModel* dst_model) = 0;
    virtual NodeDataType type() const = 0;

protected:
     NodeDelegateModel* m_src_model;
};

#endif // NODEDATAMODELTOQIODEVICECONNECTOR_H
