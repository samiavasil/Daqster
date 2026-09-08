#include<QDebug>
#include <QEnterEvent>
#include "AudioSourceDataModelUI.h"
#include "ui_AudioSourceDataModelUI.h"


AudioSourceDataModelUI::AudioSourceDataModelUI(QAudioDeviceInfo* devInfo,
                                               QAudioFormat* formatAudio,
                                               QWidget *parent) :
    QWidget(parent),
    ui(new Ui::AudioSourceDataModelUI),
    m_devInfo(devInfo),
    m_formatAudio(formatAudio),
    m_Conf(AudioCompat::AudioInputMode, *m_devInfo, *m_formatAudio)
{
    ui->setupUi(this);
    connect(&m_Conf, SIGNAL(ChangeAudioConnection(QAudioDeviceInfo ,QAudioFormat )),
            this ,SIGNAL(ChangeAudioConnection(QAudioDeviceInfo, QAudioFormat)));
    connect(ui->configButton, SIGNAL(clicked(bool)), this, SLOT(ConfigAudio()));
    connect(ui->playButton, SIGNAL(clicked(bool)), SLOT(Start(bool)));
}

AudioSourceDataModelUI::~AudioSourceDataModelUI()
{
    delete ui;
}

const QAudioFormat AudioSourceDataModelUI::FormatAudio() const
{
    return m_formatAudio ? *m_formatAudio : QAudioFormat();
}

QAudioDeviceInfo AudioSourceDataModelUI::DevInfo() const
{
    return m_devInfo ? *m_devInfo : QAudioDeviceInfo();
}

void AudioSourceDataModelUI::AudioStateChanged(QAudio::State state)
{
    ui->playButton->blockSignals(true);
    if(QAudio::StoppedState == state){
        ui->playButton->setChecked(false);
    }
    else{
        ui->playButton->setChecked(true);
    }
    ui->playButton->blockSignals(false);
}

void AudioSourceDataModelUI::Start(bool start)
{
    if(start)
        emit Start(AudioSourceDataModelUI::ASDM_START);
    else
        emit Start(AudioSourceDataModelUI::ASDM_STOP);
}



void AudioSourceDataModelUI::ConfigAudio()
{
    m_Conf.show();
    m_Conf.raise();
}

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
void AudioSourceDataModelUI::enterEvent(QEnterEvent *event)
#else
void AudioSourceDataModelUI::enterEvent(QEvent *event)
#endif
{
    if(m_Conf.isVisible()) {
        m_Conf.raise();
    }
    QWidget::enterEvent(event);
}


