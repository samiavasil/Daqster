#include "AudioSourceDataModelUI.h"
#include "ui_AudioSourceDataModelUI.h"

AudioSourceDataModelUI::AudioSourceDataModelUI(QAudio::Mode mode, QWidget *parent) :
    QWidget(parent),
    ui(new Ui::AudioSourceDataModelUI)
{
    ui->setupUi(this);

    m_devs = QAudioDeviceInfo::availableDevices(mode);
    ui->Device->setCurrentIndex(-1);
    foreach (QAudioDeviceInfo dev, m_devs) {
        ui->Device->addItem(dev.deviceName());
    }
    connect(ui->Device, SIGNAL(currentIndexChanged(int)), this, SLOT(InitAudioParams(int)),Qt::QueuedConnection);
    ui->Device->setCurrentIndex(0);
    InitAudioParams(0);
}

AudioSourceDataModelUI::~AudioSourceDataModelUI()
{
    delete ui;
}

const QAudioFormat &AudioSourceDataModelUI::FormatAudio() const
{
    return m_FormatAudio;
}

#include<QDebug>
void AudioSourceDataModelUI::InitAudioParams(int idx)
{
    QAudioDeviceInfo dInfo = m_devs[idx];
    QList<int> chanCount = dInfo.supportedChannelCounts();
    QStringList codecs = dInfo.supportedCodecs();
    QList<int> srates = dInfo.supportedSampleRates();
    QList<QAudioFormat::Endian> endians = dInfo.supportedByteOrders();
    QAudioFormat prefFormat = dInfo.preferredFormat();
    m_FormatAudio = dInfo.preferredFormat();

    qDebug() << "NAME: " << dInfo.deviceName();

    ui->ChannelNumber->clear();
    foreach (int count, chanCount) {
        ui->ChannelNumber->addItem(QString::number(count));
        if(prefFormat.channelCount() == count){
            int curIdx = ui->ChannelNumber->count() - 1;
            ui->ChannelNumber->setCurrentIndex(curIdx);
            m_FormatAudio.setChannelCount(count);
        }
    }

    ui->Codec->clear();
    foreach (QString codec, codecs) {
        ui->Codec->addItem(codec);
        if(prefFormat.codec() == codec){
            int curIdx = ui->Codec->count() - 1;
            ui->Codec->setCurrentIndex(curIdx);
            m_FormatAudio.setCodec(codec);
        }
    }

    ui->ByteOdrer->clear();
    foreach (QAudioFormat::Endian endian, endians) {
        ui->ByteOdrer->addItem(endian == QAudioFormat::LittleEndian ? "LittleEndian" :"BigEndian"  );
        if(prefFormat.byteOrder() == endian){
            int curIdx = ui->ByteOdrer->count() - 1;
            ui->ByteOdrer->setCurrentIndex(curIdx);
            m_FormatAudio.setByteOrder(endian);
        }
    }

    ui->SampleRate->clear();
    foreach (int srate, srates) {
        ui->SampleRate->addItem(QString::number(srate));
        if(prefFormat.sampleRate() == srate){
            int curIdx = ui->SampleRate->count() - 1;
            ui->SampleRate->setCurrentIndex(curIdx);
            m_FormatAudio.setSampleRate(srate);
            InitSampleAudioParams(dInfo, srate);
        }
    }

}

const char* samTypes[]={
   "Unknown", "SignedInt", "UnSignedInt", "Float"
};
void AudioSourceDataModelUI::InitSampleAudioParams(QAudioDeviceInfo& dInfo, int SampleRate){
    QAudioFormat audioFormat = m_FormatAudio;
    QAudioFormat prefFormat = dInfo.preferredFormat();
    ui->SampleSize->clear();
    ui->SampleType->clear();
    foreach (int ssize, dInfo.supportedSampleSizes()) {
        ui->SampleSize->addItem(QString::number(ssize));
        if(prefFormat.sampleSize() == ssize){
            audioFormat.setSampleSize(ssize);
            foreach (QAudioFormat::SampleType stype, dInfo.supportedSampleTypes()) {
                audioFormat.setSampleType(stype);
                ui->SampleType->addItem(samTypes[stype]);
                if(dInfo.isFormatSupported(audioFormat)){
                    if(prefFormat.sampleType() == stype){
                        ui->SampleSize->setCurrentIndex(ui->SampleSize->count() - 1);
                        ui->SampleType->setCurrentIndex(ui->SampleType->count() - 1);
                        m_FormatAudio.setSampleSize(ssize);
                        m_FormatAudio.setSampleType(stype);
                    }
                }
            }
        }
    }
}







