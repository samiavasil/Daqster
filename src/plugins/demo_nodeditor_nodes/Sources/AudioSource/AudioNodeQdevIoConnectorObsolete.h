#ifndef AUDIONODEQDEVIOCONNECTOROBSOLETE_H
#define AUDIONODEQDEVIOCONNECTOROBSOLETE_H

#include <memory>
#include <NodeDataModelToQIODeviceConnectorObsolete.h>
#include <QSharedPointer>

#include <QtNodes/NodeDelegateModel>

class QIODevice;

class AudioNodeQdevIoConnectorObsolete : public NodeDataModelToQIODeviceConnectorObsolete
{

public:

    explicit AudioNodeQdevIoConnectorObsolete( QtNodes::NodeDelegateModel* model);

    virtual QtNodes::NodeDataType type() const //TODO : TBD ???
    {
        return QtNodes::NodeDataType { "AudioNodeQdevIoConnector",
            "AudioNodeQdevIoConnector"};
    }
    virtual void ConnectModels(QtNodes::NodeDelegateModel* dst_model);
};

#endif // AUDIONODEQDEVIOCONNECTOROBSOLETE_H
