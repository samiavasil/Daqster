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

    void SetDevIo(QSharedPointer<QIODevice> dio );

    void ConnectPair( std::shared_ptr<QDevIoDisplayModel> display_model );

    virtual NodeDataType type() const
    {
        return NodeDataType { "AudioNodeQdevIoConnector",
                              "AudioNodeQdevIoConnector"};
    }

    QSharedPointer<QIODevice>& DevIo(){ return m_Devio;}

private:
    AudioSourceDataModel* m_model;
    QSharedPointer<QIODevice> m_Devio;
public slots:
    void ModChanged();
};

#endif // AUDIONODEQDEVIOCONNECTOR_H
