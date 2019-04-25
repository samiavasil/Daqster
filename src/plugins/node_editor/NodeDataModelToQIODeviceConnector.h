#ifndef NODEDATAMODELTOQIODEVICECONNECTOR_H
#define NODEDATAMODELTOQIODEVICECONNECTOR_H

#include <memory>
#include <nodes/NodeData>
#include<QObject>

class QIODevice;
using QtNodes::NodeDataType;
using QtNodes::NodeData;


class NodeDataModelToQIODeviceConnector : public QObject, public NodeData
{
public:
    NodeDataModelToQIODeviceConnector(QObject* parent = nullptr);
    virtual ~NodeDataModelToQIODeviceConnector() = default;
    virtual void SetDevIo(std::shared_ptr<QIODevice> dio) = 0;
};

#endif // NODEDATAMODELTOQIODEVICECONNECTOR_H
