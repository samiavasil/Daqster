#ifndef AUDIONODEQDEVIOCONNECTOR_H
#define AUDIONODEQDEVIOCONNECTOR_H

#include <memory>
#include <NodeDataModelToQIODeviceConnector.h>
#include <QSharedPointer>

#include <QtNodes/NodeDelegateModel>

using QtNodes::NodeDelegateModel;

class QIODevice;

class AudioNodeQdevIoConnector : public NodeDataModelToQIODeviceConnector
{

public:

    explicit AudioNodeQdevIoConnector( NodeDelegateModel* model);

    virtual NodeDataType type() const //TODO : TBD ???
    {
        return NodeDataType { "AudioNodeQdevIoConnector",
            "AudioNodeQdevIoConnector"};
    }
    virtual void ConnectModels(NodeDelegateModel* dst_model);
};

#endif // AUDIONODEQDEVIOCONNECTOR_H
