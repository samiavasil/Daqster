#ifndef AUDIONODEQDEVIOCONNECTOR_H
#define AUDIONODEQDEVIOCONNECTOR_H
#include<QObject>
#include<memory>
#include <nodes/NodeData>
#include<QSharedPointer>

using QtNodes::NodeDataType;
using QtNodes::NodeData;

class QIODevice;
class AudioSourceDataModel;
class QDevIoDisplayModel;

class AudioNodeQdevIoConnector : public QObject, public NodeData
{
    Q_OBJECT
public:

    explicit AudioNodeQdevIoConnector( AudioSourceDataModel* model, QObject *parent=nullptr );

    void SetDevIo(std::shared_ptr<QIODevice> dio );

    void ConnectPair( std::shared_ptr<QDevIoDisplayModel> display_model );

    virtual NodeDataType type() const
    {
        return NodeDataType { "AudioNodeQdevIoConnector",
            "AudioNodeQdevIoConnector"};
    }

    std::shared_ptr<QIODevice>& DevIo(){ return m_Devio;}

private:
    AudioSourceDataModel* m_model;
    std::shared_ptr<QIODevice> m_Devio;
public slots:
    void ModChanged();
};

#endif // AUDIONODEQDEVIOCONNECTOR_H
