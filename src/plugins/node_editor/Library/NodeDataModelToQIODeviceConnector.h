#ifndef NODEDATAMODELTOQIODEVICECONNECTOR_H
#define NODEDATAMODELTOQIODEVICECONNECTOR_H

#include <memory>
#include <nodes/NodeData>
#include<nodes/NodeDataModel>

using QtNodes::NodeDataModel;

class QIODevice;
using QtNodes::NodeDataType;
using QtNodes::NodeData;


class NodeDataModelToQIODeviceConnector :   public NodeData
{
public:
    NodeDataModelToQIODeviceConnector( NodeDataModel* model);
    virtual ~NodeDataModelToQIODeviceConnector() = default;
    virtual void ConnectModels(NodeDataModel* dst_model) = 0;
    virtual NodeDataType type() const = 0; //TODO : TBD ???

protected:
     NodeDataModel* m_src_model;
};

#endif // NODEDATAMODELTOQIODEVICECONNECTOR_H
