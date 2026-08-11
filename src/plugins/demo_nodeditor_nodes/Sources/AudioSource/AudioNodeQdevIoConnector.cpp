#include <XYSeriesIODeviceObsolete.h>
#include <AudioNodeQdevIoConnector.h>
#include <AudioSourceDataModel.h>
#include <QDevIoDisplayModelObsolete.h>

#include <QDebug>

AudioNodeQdevIoConnector::AudioNodeQdevIoConnector(QtNodes::NodeDelegateModel* model) : NodeDataModelToQIODeviceConnectorObsolete(model) {
}

void AudioNodeQdevIoConnector::ConnectModels(QtNodes::NodeDelegateModel* dst_model) {
  AudioSourceDataModel* model_src = dynamic_cast<AudioSourceDataModel*>(m_src_model);
  QDevIoDisplayModelObsolete* model_dst = dynamic_cast<QDevIoDisplayModelObsolete*>(dst_model);

  if (nullptr != model_src) {
    std::shared_ptr<QIODevice> xDevio = model_dst->device();

    if (nullptr != xDevio) {
      QObject::connect(model_src, SIGNAL(ChangeAudioConnection(QAudioDeviceInfo, QAudioFormat)),
                       model_dst, SLOT(ChangeAudioConnection(QAudioDeviceInfo, QAudioFormat)));

      if (!xDevio->isOpen()) {
        xDevio->open(QIODevice::WriteOnly);
      }
    }
    model_src->IO_connect(xDevio);
  }
}
