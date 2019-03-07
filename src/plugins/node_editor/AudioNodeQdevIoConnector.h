#ifndef AUDIONODEQDEVIOCONNECTOR_H
#define AUDIONODEQDEVIOCONNECTOR_H
#include<memory>

#include <nodes/NodeData>

using QtNodes::NodeDataType;
using QtNodes::NodeData;

class QIODevice;
class AudioSourceDataModel;

class AudioNodeQdevIoConnector : public NodeData
{

public:

    explicit AudioNodeQdevIoConnector( AudioSourceDataModel* model );

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

};

#endif // AUDIONODEQDEVIOCONNECTOR_H
