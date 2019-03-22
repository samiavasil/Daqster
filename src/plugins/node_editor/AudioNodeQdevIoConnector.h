#ifndef AUDIONODEQDEVIOCONNECTOR_H
#define AUDIONODEQDEVIOCONNECTOR_H
#include<QObject>
#include<memory>
#include <nodes/NodeData>

using QtNodes::NodeDataType;
using QtNodes::NodeData;

class QIODevice;
class AudioSourceDataModel;

class AudioNodeQdevIoConnector : public QObject, public NodeData
{
    Q_OBJECT
public:

    explicit AudioNodeQdevIoConnector( AudioSourceDataModel* model, QObject *parent=nullptr );

    void SetDevIo( std::shared_ptr<QIODevice> dio );

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
