#ifndef AUDIOSOURCEDATAMODELUI_H
#define AUDIOSOURCEDATAMODELUI_H

#include "AudioCompat.h"
#include "AudioStartStop.h"

#include <QWidget>
#include "AudioSourceConfig.h"

namespace Ui {
class AudioSourceDataModelUI;
}

class AudioSourceDataModelUI : public QWidget
{
    Q_OBJECT

public:
    // REQ-SW-PL-051: the Start/Stop command type (formerly
    // AudioStartStop) now lives in AudioStartStop.h in the
    // core plugin, so the core models can use it without depending on this
    // GUI class. ASDM_STOP / ASDM_START / ASDM_RELOAD are unchanged.

    explicit AudioSourceDataModelUI(QAudioDeviceInfo* devInfo,
                                    QAudioFormat* formatAudio,
                                    QWidget *parent = nullptr);
    ~AudioSourceDataModelUI();
    const QAudioFormat FormatAudio() const;

    QAudioDeviceInfo DevInfo() const;

signals:
    void ChangeAudioConnection(QAudioDeviceInfo devInfo, QAudioFormat formatAudio);
    void Start(AudioStartStop start);

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
