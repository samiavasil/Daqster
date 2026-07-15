#include<QDebug>
#include<QMessageBox>
#include "AudioSourceConfig.h"
#include "ui_AudioSourceConfig.h"

AudioSourceConfig::AudioSourceConfig(AudioCompat::Mode mode,
                                     QAudioDeviceInfo &devInfo,
                                     QAudioFormat &formatAudio,
                                     QWidget *parent) :
    QWidget(parent),
    ui(new Ui::AudioSourceConfig),
    m_DevInfo(&devInfo),
    m_FormatAudio(&formatAudio),
    m_Mode(mode)
{
    ui->setupUi(this);

    // Callbacks shared by all models.
    // currentFormat: takes the current QAudioFormat so flags() can build a
    //                test-format that differs only in the field being checked.
    // isSupported:   delegates to AudioSourceConfig::isFormatSupported which
    //                asks the currently selected QAudioDeviceInfo.
    auto currentFormat = [this]() -> QAudioFormat {
        return m_FormatAudio ? *m_FormatAudio : QAudioFormat();
    };
    auto isSupported = [this](const QAudioFormat& f) -> bool {
        return isFormatSupported(f);
    };

    m_ChannelModel    = new QAudioComboModel(currentFormat, isSupported, QAudioComboModel::CHANNEL_NUMBER, this);
    m_CodecModel      = new QAudioComboModel(currentFormat, isSupported, QAudioComboModel::CODEC,          this);
    m_ByteOrderModel  = new QAudioComboModel(currentFormat, isSupported, QAudioComboModel::BYTES_ORDER,    this);
    m_SampleRateModel = new QAudioComboModel(currentFormat, isSupported, QAudioComboModel::SAMPLE_RATE,    this);
    m_SampleSizeModel = new QAudioComboModel(currentFormat, isSupported, QAudioComboModel::SAMPLE_SIZE,    this);
    m_SampleTypeModel = new QAudioComboModel(currentFormat, isSupported, QAudioComboModel::SAMPLE_TYPE,    this);

    ui->ChannelNumber->setModel(m_ChannelModel);
    ui->Codec->setModel(m_CodecModel);
    ui->ByteOdrer->setModel(m_ByteOrderModel);
    ui->SampleRate->setModel(m_SampleRateModel);
    ui->SampleSize->setModel(m_SampleSizeModel);
    ui->SampleType->setModel(m_SampleTypeModel);

    connect(ui->ChannelNumber, SIGNAL(currentIndexChanged(int)), this, SLOT(ChannelNumberChanged(int)));
    connect(ui->Codec,         SIGNAL(currentIndexChanged(int)), this, SLOT(CodecChanged(int)));
    connect(ui->ByteOdrer,     SIGNAL(currentIndexChanged(int)), this, SLOT(ByteOdrerChanged(int)));
    connect(ui->SampleRate,    SIGNAL(currentIndexChanged(int)), this, SLOT(SampleRateChanged(int)));
    connect(ui->SampleSize,    SIGNAL(currentIndexChanged(int)), this, SLOT(SampleSizeChanged(int)));
    connect(ui->SampleType,    SIGNAL(currentIndexChanged(int)), this, SLOT(SampleTypeChanged(int)));

    qDebug() << "this: " << this << " Mode: " << m_Mode;
    connect(ui->Device, SIGNAL(currentIndexChanged(int)), this, SLOT(ABEInitAudioParams(int)));
}

AudioSourceConfig::~AudioSourceConfig()
{
    delete ui;
}

const QAudioFormat &AudioSourceConfig::FormatAudio() const
{
    Q_ASSERT(m_FormatAudio != nullptr);
    return *m_FormatAudio;
}

void AudioSourceConfig::ABEInitAudioParams(int idx)
{
    if (idx < 0 || idx >= m_Devs.count() || !m_DevInfo) return;

    *m_DevInfo = m_Devs[idx];

    // Block combo signals while we repopulate so the Changed-slots don't fire.
    ui->ChannelNumber->blockSignals(true);
    ui->Codec->blockSignals(true);
    ui->ByteOdrer->blockSignals(true);
    ui->SampleRate->blockSignals(true);
    ui->SampleSize->blockSignals(true);
    ui->SampleType->blockSignals(true);

    qDebug() << "NAME: " << AudioCompat::deviceName(*m_DevInfo);

    // Helper: build a QList<QVariant> and find the index of the currently
    // active value so the combo can be pre-selected after populate().

    // --- Channel counts ---
    {
        QList<QVariant> data;
        int sel = -1;
        for (int ch : AudioCompat::supportedChannelCounts(*m_DevInfo)) {
            if (m_FormatAudio->channelCount() == ch) sel = data.count();
            data << ch;
        }
        m_ChannelModel->populate(data);
        int setIdx = (sel >= 0) ? sel : 0;
        ui->ChannelNumber->setCurrentIndex(setIdx);
        if (!data.isEmpty())
            m_FormatAudio->setChannelCount(data[setIdx].toInt());
    }

    // --- Codecs ---
    {
        QList<QVariant> data;
        int sel = -1;
        for (const QString& codec : AudioCompat::supportedCodecs(*m_DevInfo)) {
            if (AudioCompat::codec(*m_FormatAudio) == codec) sel = data.count();
            data << codec;
        }
        m_CodecModel->populate(data);
        int setIdx = (sel >= 0) ? sel : 0;
        ui->Codec->setCurrentIndex(setIdx);
        if (!data.isEmpty())
            AudioCompat::setCodec(*m_FormatAudio, data[setIdx].toString());
    }

    // --- Byte orders ---
    {
        QList<QVariant> data;
        int sel = -1;
        for (AudioCompat::Endian e : AudioCompat::supportedByteOrders(*m_DevInfo)) {
            if (AudioCompat::byteOrder(*m_FormatAudio) == e) sel = data.count();
            data << static_cast<int>(e);
        }
        m_ByteOrderModel->populate(data);
        int setIdx = (sel >= 0) ? sel : 0;
        ui->ByteOdrer->setCurrentIndex(setIdx);
        if (!data.isEmpty())
            AudioCompat::setByteOrder(
                *m_FormatAudio,
                static_cast<AudioCompat::Endian>(data[setIdx].toInt()));
    }

    // --- Sample rates ---
    {
        QList<QVariant> data;
        int sel = -1;
        for (int sr : AudioCompat::supportedSampleRates(*m_DevInfo)) {
            if (m_FormatAudio->sampleRate() == sr) sel = data.count();
            data << sr;
        }
        m_SampleRateModel->populate(data);
        int setIdx = (sel >= 0) ? sel : 0;
        ui->SampleRate->setCurrentIndex(setIdx);
        if (!data.isEmpty())
            m_FormatAudio->setSampleRate(data[setIdx].toInt());
    }

    // --- Sample sizes ---
    {
        QList<QVariant> data;
        int sel = -1;
        for (int ss : AudioCompat::supportedSampleSizes(*m_DevInfo)) {
            if (AudioCompat::sampleSize(*m_FormatAudio) == ss) sel = data.count();
            data << ss;
        }
        m_SampleSizeModel->populate(data);
        int setIdx = (sel >= 0) ? sel : 0;
        ui->SampleSize->setCurrentIndex(setIdx);
        if (!data.isEmpty())
            AudioCompat::setSampleSize(*m_FormatAudio, data[setIdx].toInt());
    }

    // --- Sample types ---
    {
        QList<QVariant> data;
        int sel = -1;
        for (AudioCompat::SampleType st : AudioCompat::supportedSampleTypes(*m_DevInfo)) {
            if (AudioCompat::sampleType(*m_FormatAudio) == st) sel = data.count();
            data << static_cast<int>(st);
        }
        m_SampleTypeModel->populate(data);
        int setIdx = (sel >= 0) ? sel : 0;
        ui->SampleType->setCurrentIndex(setIdx);
        if (!data.isEmpty())
            AudioCompat::setSampleType(
                *m_FormatAudio,
                static_cast<AudioCompat::SampleType>(data[setIdx].toInt()));
    }

    ui->ChannelNumber->blockSignals(false);
    ui->Codec->blockSignals(false);
    ui->ByteOdrer->blockSignals(false);
    ui->SampleRate->blockSignals(false);
    ui->SampleSize->blockSignals(false);
    ui->SampleType->blockSignals(false);

    if (m_DevInfo && m_FormatAudio) {
        emit ChangeAudioConnection(*m_DevInfo, *m_FormatAudio);
    }
}


bool AudioSourceConfig::isFormatSupported(const QAudioFormat &format) const{
    bool ret = false;

    if(ui->Device->currentIndex() >= 0 && m_DevInfo){
        ret = AudioCompat::isFormatSupported(m_Devs[ui->Device->currentIndex()], format);
    }
    return ret;
}

void AudioSourceConfig::show()
{
    qDebug() << "this: " << this << " Mode: " << m_Mode;

    m_Devs = AudioCompat::availableDevices(m_Mode);
    ui->Device->blockSignals(true);
    ui->Device->clear();
    int idx = -1;
	if (m_Devs.isEmpty())
	{
	    const QAudioDeviceInfo inputDevice = AudioCompat::defaultInputDevice();
	    if (AudioCompat::isNull(inputDevice)) {
	        QMessageBox::warning(nullptr, "audio",
	                             "There is no audio input device available.");
	        return ;
	    }
	    m_Devs.append(inputDevice);
	}
    foreach (QAudioDeviceInfo dev, m_Devs) {
        ui->Device->addItem(AudioCompat::deviceName(dev));
        if(m_DevInfo && AudioCompat::deviceName(*m_DevInfo) == AudioCompat::deviceName(dev)){
            qDebug() << "Defaul dev name: " << AudioCompat::deviceName(*m_DevInfo) << ":" << AudioCompat::deviceName(dev) ;
            idx = ui->Device->count() - 1;
            qDebug()<<"idx: " << idx;
        }
    }
    ui->Device->setCurrentIndex(idx);
    Q_ASSERT(m_DevInfo && *m_DevInfo == m_Devs[idx]);
    ui->Device->blockSignals(false);

    ABEInitAudioParams(idx);
    QWidget::show();
}

void AudioSourceConfig::ChannelNumberChanged(int val){
    qDebug() << "val: " << val;
    if (m_FormatAudio) m_FormatAudio->setChannelCount(ui->ChannelNumber->itemData(val, Qt::DisplayRole).toInt());
    qDebug() << __FUNCTION__ << ui->ChannelNumber->itemData(val, Qt::DisplayRole).toInt();
    if (m_DevInfo && m_FormatAudio) emit ChangeAudioConnection(*m_DevInfo, *m_FormatAudio);
}

void AudioSourceConfig::CodecChanged(int val){
    qDebug() << "val: " << val;
    if (m_FormatAudio) AudioCompat::setCodec(*m_FormatAudio, ui->Codec->itemData(val, Qt::DisplayRole).toString());
    qDebug() << __FUNCTION__ << ui->Codec->itemData(val, Qt::DisplayRole).toString();
    if (m_DevInfo && m_FormatAudio) emit ChangeAudioConnection(*m_DevInfo, *m_FormatAudio);
}

void AudioSourceConfig::ByteOdrerChanged(int val){
    qDebug() << "val: " << val;
    if (m_FormatAudio)
        AudioCompat::setByteOrder(
            *m_FormatAudio,
            static_cast<AudioCompat::Endian>(ui->ByteOdrer->itemData(val, Qt::UserRole).toInt()));
    qDebug() << __FUNCTION__ << ui->ByteOdrer->itemData(val, Qt::UserRole).toInt();
    if (m_DevInfo && m_FormatAudio) emit ChangeAudioConnection(*m_DevInfo, *m_FormatAudio);
}

void AudioSourceConfig::SampleRateChanged(int val){
    qDebug() << "val: " << val;
    if (m_FormatAudio) m_FormatAudio->setSampleRate(ui->SampleRate->itemData(val, Qt::DisplayRole).toInt());
    qDebug() << __FUNCTION__ << ui->SampleRate->itemData(val, Qt::DisplayRole).toInt();
    if (m_DevInfo && m_FormatAudio) emit ChangeAudioConnection(*m_DevInfo, *m_FormatAudio);
}

void AudioSourceConfig::SampleSizeChanged(int val){
    qDebug() << "val: " << val;
    if (m_FormatAudio) AudioCompat::setSampleSize(*m_FormatAudio, ui->SampleSize->itemData(val, Qt::DisplayRole).toInt());
    qDebug() << __FUNCTION__ << ui->SampleSize->itemData(val, Qt::DisplayRole).toInt();
    if (m_DevInfo && m_FormatAudio) emit ChangeAudioConnection(*m_DevInfo, *m_FormatAudio);
}

void AudioSourceConfig::SampleTypeChanged(int val){
    qDebug() << "val: " << val;
    if (m_FormatAudio)
        AudioCompat::setSampleType(
            *m_FormatAudio,
            static_cast<AudioCompat::SampleType>(ui->SampleType->itemData(val, Qt::UserRole).toInt()));
    qDebug() << __FUNCTION__ << ui->SampleType->itemData(val, Qt::UserRole).toInt();
    if (m_DevInfo && m_FormatAudio) emit ChangeAudioConnection(*m_DevInfo, *m_FormatAudio);
}

