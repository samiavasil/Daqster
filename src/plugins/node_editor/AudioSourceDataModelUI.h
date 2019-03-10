#ifndef AUDIOSOURCEDATAMODELUI_H
#define AUDIOSOURCEDATAMODELUI_H

#include <QWidget>
#include <QtMultimedia/QAudioDeviceInfo>

namespace Ui {
class AudioSourceDataModelUI;
}
class QAudioFormat;
class AudioSourceDataModelUI : public QWidget
{
    Q_OBJECT

public:
    explicit AudioSourceDataModelUI(QAudio::Mode mode = QAudio::AudioInput, QWidget *parent = 0);
    ~AudioSourceDataModelUI();


    const QAudioFormat& FormatAudio() const;

protected:
    void InitSampleAudioParams(QAudioDeviceInfo &dInfo, int SampleRate);
protected slots:
    void InitAudioParams(int idx);
private:
    Ui::AudioSourceDataModelUI *ui;
    QAudioFormat     m_FormatAudio;
    QList<QAudioDeviceInfo> m_devs;
};

#endif // AUDIOSOURCEDATAMODELUI_H
