#ifndef AUDIOSOURCEDATAMODELUI_H
#define AUDIOSOURCEDATAMODELUI_H

#include "AudioCompat.h"

#include <QWidget>
#include "AudioSourceConfig.h"

namespace Ui {
class AudioSourceDataModelUI;
}

class AudioSourceDataModelUI : public QWidget
{
    Q_OBJECT

public:
    // Shared Start/Stop command type used by both the SampledData
    // AudioSourceDataModel and the obsolete QDevIO AudioSourceDataModelObsolete
    // (REQ-SW-PL-024). Previously it lived in the model header; it was moved
    // here so the shared UI defines the single contract both nodes wire to.
    enum StartStop {
        ASDM_STOP,
        ASDM_START,
        ASDM_RELOAD,
    };

    explicit AudioSourceDataModelUI(QAudioDeviceInfo* devInfo,
                                    QAudioFormat* formatAudio,
                                    QWidget *parent = nullptr);
    ~AudioSourceDataModelUI();
    const QAudioFormat FormatAudio() const;

    QAudioDeviceInfo DevInfo() const;

signals:
    void ChangeAudioConnection(QAudioDeviceInfo devInfo, QAudioFormat formatAudio);
    void Start(AudioSourceDataModelUI::StartStop start);

public slots:
    void AudioStateChanged(QAudio::State state);
private slots:
    void Start(bool start);
    void ConfigAudio();
protected:
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    virtual void enterEvent(QEnterEvent *event) override;
#else
    virtual void enterEvent(QEvent *event);
#endif
private:
    Ui::AudioSourceDataModelUI *ui;
    QAudioDeviceInfo* m_devInfo = nullptr;       // non-owning: externally managed
    QAudioFormat* m_formatAudio = nullptr;       // non-owning: externally managed
    AudioSourceConfig m_Conf;
};

#endif // AUDIOSOURCEDATAMODELUI_H
