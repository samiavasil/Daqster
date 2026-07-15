#ifndef NODEDATAMODELTOQIODEVICECONNECTOR_H
#define NODEDATAMODELTOQIODEVICECONNECTOR_H

#include <memory>
#include <QtNodes/NodeData>
#include <QtNodes/NodeDelegateModel>

class QIODevice;

class NodeDataModelToQIODeviceConnector : public QtNodes::NodeData
{
public:
    NodeDataModelToQIODeviceConnector( QtNodes::NodeDelegateModel* model);
    virtual ~NodeDataModelToQIODeviceConnector() = default;
    virtual void ConnectModels(QtNodes::NodeDelegateModel* dst_model) = 0;
    virtual QtNodes::NodeDataType type() const = 0;

protected:
     QtNodes::NodeDelegateModel* m_src_model;
};

#endif // NODEDATAMODELTOQIODEVICECONNECTOR_H
