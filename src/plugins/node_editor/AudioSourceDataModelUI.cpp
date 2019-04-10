#include<QDebug>
#include "AudioSourceDataModelUI.h"
#include "ui_AudioSourceDataModelUI.h"


AudioSourceDataModelUI::AudioSourceDataModelUI(QAudioDeviceInfo& devInfo,
                                               QAudioFormat& formatAudio,
                                               QWidget *parent) :
    QWidget(parent),
    ui(new Ui::AudioSourceDataModelUI),
    m_Conf(QAudio::AudioInput, devInfo, formatAudio),
    m_devInfo(devInfo),
    m_formatAudio(formatAudio)
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
    return QAudioFormat();
}

QAudioDeviceInfo AudioSourceDataModelUI::DevInfo() const
{
    return QAudioDeviceInfo();
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
        emit Start(AudioSourceDataModel::ASDM_START);
    else
        emit Start(AudioSourceDataModel::ASDM_STOP);
}



void AudioSourceDataModelUI::ConfigAudio()
{
    m_Conf.show();
}

