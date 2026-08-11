#ifndef AUDIONODEQDEVIOCONNECTOR_H
#define AUDIONODEQDEVIOCONNECTOR_H

#include <memory>
#include <NodeDataModelToQIODeviceConnectorObsolete.h>
#include <QSharedPointer>

#include <QtNodes/NodeDelegateModel>

class QIODevice;

class AudioNodeQdevIoConnector : public NodeDataModelToQIODeviceConnectorObsolete
{

public:

    explicit AudioNodeQdevIoConnector( QtNodes::NodeDelegateModel* model);

    virtual QtNodes::NodeDataType type() const //TODO : TBD ???
    {
        return QtNodes::NodeDataType { "AudioNodeQdevIoConnector",
            "AudioNodeQdevIoConnector"};
    }
    virtual void ConnectModels(QtNodes::NodeDelegateModel* dst_model);
};

#endif // AUDIONODEQDEVIOCONNECTOR_H
