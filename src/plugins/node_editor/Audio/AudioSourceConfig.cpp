#include<QDebug>
#include "AudioSourceConfig.h"
#include "ui_AudioSourceConfig.h"



const QMap<QAudioFormat::SampleType, QString> cTypeMap{
    {QAudioFormat::Unknown,     "Unknown"},
    {QAudioFormat::SignedInt,   "SignedInt"},
    {QAudioFormat::UnSignedInt, "UnSignedInt"},
    {QAudioFormat::Float,       "Float"}
};

const QMap<QAudioFormat::Endian, QString> cEndianMap{
    {QAudioFormat::LittleEndian, "LittleEndian"},
    {QAudioFormat::BigEndian,    "BigEndian"},
};

class QAudioComboModel:public QAbstractListModel{
public:
    enum AudioModelType {
        CHANNEL_NUMBER,
        CODEC,
        BYTES_ORDER,
        SAMPLE_RATE,
        SAMPLE_SIZE,
        SAMPLE_TYPE
    };

    explicit QAudioComboModel(const AudioSourceConfig& model, AudioModelType type, QObject *parent = nullptr):
        QAbstractListModel(parent),
        m_model(model),
        m_Type(type)
    {

    }

    virtual int rowCount(const QModelIndex &parent = QModelIndex())  const{
        if (parent.isValid())
            return 0;
        else
            return m_Data.count();
    }

    virtual bool insertRows(int row, int count, const QModelIndex &parent = QModelIndex()){
        if (parent.isValid())
            return false;
        blockSignals(true);
        beginInsertRows(parent, row, row + count - 1);
        for(int i = row; i < row+count; i++){
            m_Data.insert(row, QVariant());
        }
        endInsertRows();
        blockSignals(false);
        return true;
    }

    virtual bool removeRows(int row, int count, const QModelIndex &parent = QModelIndex()){
        if (parent.isValid())
            return false;
        blockSignals(true);
        beginRemoveRows(parent, row, row + count - 1);
        for(int i = row; i < row+count; i++){
            m_Data.removeAt(row);
        }
        endRemoveRows();
        blockSignals(false);
        return true;
    }

    Qt::ItemFlags flags(const QModelIndex &index) const{

        Qt::ItemFlags flags = Qt::NoItemFlags;
        if (index.isValid()){

            QAudioFormat audioFormat = m_model.FormatAudio();
            qDebug() << "row" << index.row();
            switch (m_Type) {
            case CHANNEL_NUMBER:{
                audioFormat.setChannelCount(m_Data.value(index.row(),QString("BAD CHANNEL")).toInt());
                break;
            }
            case CODEC:{
                audioFormat.setCodec(m_Data.value(index.row(),QString("BAD CODEC")).toString());
                break;
            }
            case BYTES_ORDER:{
                audioFormat.setByteOrder(cEndianMap.key(m_Data.value(index.row(),QString("BAD ORDER")).toString()));
                break;
            }
            case SAMPLE_RATE:{
                audioFormat.setSampleRate(m_Data.value(index.row(),QString("BAD SRATE")).toInt());
                break;
            }
            case SAMPLE_SIZE:{
                audioFormat.setSampleSize(m_Data.value(index.row(),QString("BAD SSIZE")).toInt());
                break;
            }
            case SAMPLE_TYPE:{
                audioFormat.setSampleType(cTypeMap.key(m_Data.value(index.row(),QString("BAD STYPE")).toString()));
                break;
            }
            default:
                break;
            }
            qDebug() << audioFormat;

            if(m_model.isFormatSupported(audioFormat))
            {
                flags = Qt::ItemIsEnabled | Qt::ItemIsSelectable;
            }
        }
        return flags;
    }

    virtual QVariant data(const QModelIndex &index, int role) const{
        Q_ASSERT(m_Data.count() > index.row());
        Q_ASSERT(0==index.column());
        if (index.isValid() &&
                (role == Qt::DisplayRole||
                 role == Qt::WhatsThisRole))
            return m_Data.value(index.row(),QString("BadBoy"));
        else
            return QVariant();

    }

    virtual bool setData(const QModelIndex &index, const QVariant &value, int role = Qt::EditRole){
        Q_ASSERT(m_Data.count() > index.row());
        Q_ASSERT(0==index.column());
        if (role == Qt::EditRole)
        {
            m_Data[index.row()] = value.toString();
        }
        return true;
    }


protected:
    const AudioSourceConfig& m_model;
    QList<QVariant> m_Data;
    AudioModelType m_Type;
};


AudioSourceConfig::AudioSourceConfig(QAudio::Mode mode,
                                     QAudioDeviceInfo &devInfo,
                                     QAudioFormat &formatAudio,
                                     QWidget *parent) :
    QWidget(parent),
    ui(new Ui::AudioSourceConfig),
    m_FormatAudio(formatAudio),
    m_DevInfo(devInfo),
    m_Mode(mode)
{

    ui->setupUi(this);

   //  QAudioComboModel* model = new QAudioComboModel(*this, QAudioComboModel::CHANNEL_NUMBER);
 //      ui->ChannelNumber->setModel(model); //TODO: FIX THIS to use a models or remove QAudioComboModel at all
    connect(ui->ChannelNumber, SIGNAL(currentIndexChanged(int)), this, SLOT(ChannelNumberChanged(int)));
  //   model = new QAudioComboModel(*this, QAudioComboModel::CODEC);
 //        ui->Codec->setModel(model);
    connect(ui->Codec, SIGNAL(currentIndexChanged(int)), this, SLOT(CodecChanged(int)));
  //   model = new QAudioComboModel(*this, QAudioComboModel::BYTES_ORDER);
 //      ui->ByteOdrer->setModel(model);
    connect(ui->ByteOdrer, SIGNAL(currentIndexChanged(int)), this, SLOT(ByteOdrerChanged(int)));
  //   model = new QAudioComboModel(*this, QAudioComboModel::SAMPLE_RATE);
 //       ui->SampleRate->setModel(model);
    connect(ui->SampleRate, SIGNAL(currentIndexChanged(int)), this, SLOT(SampleRateChanged(int)));
  //   model = new QAudioComboModel(*this, QAudioComboModel::SAMPLE_SIZE);
 //          ui->SampleSize->setModel(model);
    connect(ui->SampleSize, SIGNAL(currentIndexChanged(int)), this, SLOT(SampleSizeChanged(int)));
 //    model = new QAudioComboModel(*this, QAudioComboModel::SAMPLE_TYPE);
 //        ui->SampleType->setModel(model);
    connect(ui->SampleType, SIGNAL(currentIndexChanged(int)), this, SLOT(SampleTypeChanged(int)));

    qDebug() << "this: " << this << " Mode: " << m_Mode;
    connect(ui->Device, SIGNAL(currentIndexChanged(int)), this, SLOT(InitAudioParams(int)));
}

AudioSourceConfig::~AudioSourceConfig()
{
    delete ui;
}

const QAudioFormat &AudioSourceConfig::FormatAudio() const
{
    return m_FormatAudio;
}

void AudioSourceConfig::InitAudioParams(int idx)
{

    m_DevInfo = m_Devs[idx];
    QList<int> chanCount = m_DevInfo.supportedChannelCounts();
    QStringList codecs = m_DevInfo.supportedCodecs();
    QList<int> srates = m_DevInfo.supportedSampleRates();
    QList<QAudioFormat::Endian> endians = m_DevInfo.supportedByteOrders();

    m_FormatAudio = m_DevInfo.preferredFormat();
    ui->ChannelNumber->blockSignals(true);
    ui->Codec->blockSignals(true);
    ui->ByteOdrer->blockSignals(true);
    ui->SampleRate->blockSignals(true);
    ui->SampleSize->blockSignals(true);
    ui->SampleType->blockSignals(true);

    qDebug() << "NAME: " << m_DevInfo.deviceName();
    blockSignals(true);
    ui->ChannelNumber->clear();
    for(int i = 0; i < chanCount.count(); i++) {
        ui->ChannelNumber->addItem(QString::number(chanCount[i]));
        if(m_FormatAudio.channelCount() == chanCount[i]){
            ui->ChannelNumber->setCurrentIndex( i );
            m_FormatAudio.setChannelCount(chanCount[i]);
        }
    }

    ui->Codec->clear();
    foreach (QString codec, codecs) {
        ui->Codec->addItem(codec);
        if(m_FormatAudio.codec() == codec){
            int curIdx = ui->Codec->count() - 1;
            ui->Codec->setCurrentIndex(curIdx);
            m_FormatAudio.setCodec(codec);
        }
    }

    ui->ByteOdrer->clear();
    foreach (QAudioFormat::Endian endian, endians) {
        ui->ByteOdrer->addItem(endian == QAudioFormat::LittleEndian ?
                                   cEndianMap.value(QAudioFormat::LittleEndian):
                                   cEndianMap.value(QAudioFormat::BigEndian));
        if(m_FormatAudio.byteOrder() == endian){
            int curIdx = ui->ByteOdrer->count() - 1;
            ui->ByteOdrer->setCurrentIndex(curIdx);
            m_FormatAudio.setByteOrder(endian);
        }
    }

    ui->SampleRate->clear();
    foreach (int srate, srates) {
        ui->SampleRate->addItem(QString::number(srate));
        if(m_FormatAudio.sampleRate() == srate){
            int curIdx = ui->SampleRate->count() - 1;
            ui->SampleRate->setCurrentIndex(curIdx);
            m_FormatAudio.setSampleRate(srate);
        }
    }

    ui->SampleSize->clear();
    foreach (int ssize, m_DevInfo.supportedSampleSizes()) {
        ui->SampleSize->addItem(QString::number(ssize));
        if(m_FormatAudio.sampleSize() == ssize){
            ui->SampleSize->setCurrentIndex(ui->SampleSize->count() - 1);
            m_FormatAudio.setSampleSize(ssize);
        }
    }

    ui->SampleType->clear();
    foreach (QAudioFormat::SampleType stype, m_DevInfo.supportedSampleTypes()) {
        ui->SampleType->addItem(cTypeMap.value(stype));
        if(m_FormatAudio.sampleType() == stype){
            ui->SampleType->setCurrentIndex(ui->SampleType->count() - 1);
            m_FormatAudio.setSampleType(stype);
        }
    }
    blockSignals(false);
    ui->ChannelNumber->blockSignals(false);
    ui->Codec->blockSignals(false);
    ui->ByteOdrer->blockSignals(false);
    ui->SampleRate->blockSignals(false);
    ui->SampleSize->blockSignals(false);
    ui->SampleType->blockSignals(false);
//    emit ChangeAudioConnection(m_DevInfo, m_FormatAudio);

}


bool AudioSourceConfig::isFormatSupported(const QAudioFormat &format) const{
    bool ret = false;

    if(ui->Device->currentIndex() >= 0){
        ret = m_Devs[ui->Device->currentIndex()].isFormatSupported(format);
    }
    return ret;
}

void AudioSourceConfig::show()
{
    qDebug() << "this: " << this << " Mode: " << m_Mode;

    m_Devs = QAudioDeviceInfo::availableDevices(m_Mode);
    ui->Device->blockSignals(true);
    ui->Device->clear();
    int idx = -1;

    foreach (QAudioDeviceInfo dev, m_Devs) {
        ui->Device->addItem(dev.deviceName());
        if(m_DevInfo.deviceName() == dev.deviceName()){
            qDebug() << "Defaul dev name: " << m_DevInfo.deviceName() << ":" <<dev.deviceName() ;
            idx = ui->Device->count() - 1;
            qDebug()<<"idx: " << idx;
        }
    }
    ui->Device->setCurrentIndex(idx);
    Q_ASSERT(m_DevInfo == m_Devs[idx]);
    ui->Device->blockSignals(false);

    InitAudioParams(idx);
    QWidget::show();
}

void AudioSourceConfig::ChannelNumberChanged(int val){
    qDebug() << "val: " << val;
    m_FormatAudio.setChannelCount(ui->ChannelNumber->itemData(val, Qt::DisplayRole).toInt());
    qDebug() << __FUNCTION__ << ui->ChannelNumber->itemData(val, Qt::DisplayRole).toInt();
    emit ChangeAudioConnection(m_DevInfo, m_FormatAudio);
}

void AudioSourceConfig::CodecChanged(int val){
    qDebug() << "val: " << val;
    m_FormatAudio.setCodec(ui->Codec->itemData(val, Qt::DisplayRole).toString());
    qDebug() << __FUNCTION__ << ui->Codec->itemData(val, Qt::DisplayRole).toString();
    emit ChangeAudioConnection(m_DevInfo, m_FormatAudio);
}

void AudioSourceConfig::ByteOdrerChanged(int val){
    qDebug() << "val: " << val;
    m_FormatAudio.setByteOrder(cEndianMap.key(ui->ByteOdrer->itemData(val, Qt::DisplayRole).toString()));
    qDebug() << __FUNCTION__ << cEndianMap.key(ui->ByteOdrer->itemData(val, Qt::DisplayRole).toString());
    emit ChangeAudioConnection(m_DevInfo, m_FormatAudio);
}

void AudioSourceConfig::SampleRateChanged(int val){
    qDebug() << "val: " << val;
    m_FormatAudio.setSampleRate(ui->SampleRate->itemData(val, Qt::DisplayRole).toInt());
    qDebug() << __FUNCTION__ << ui->SampleRate->itemData(val, Qt::DisplayRole).toInt();
    emit ChangeAudioConnection(m_DevInfo, m_FormatAudio);
}

void AudioSourceConfig::SampleSizeChanged(int val){
    qDebug() << "val: " << val;
    m_FormatAudio.setSampleSize(ui->SampleSize->itemData(val, Qt::DisplayRole).toInt());
    qDebug() << __FUNCTION__ << ui->SampleSize->itemData(val, Qt::DisplayRole).toInt();
    emit ChangeAudioConnection(m_DevInfo, m_FormatAudio);
}

void AudioSourceConfig::SampleTypeChanged(int val){
    qDebug() << "val: " << val;
    m_FormatAudio.setSampleType(cTypeMap.key(ui->SampleType->itemData(val, Qt::DisplayRole).toString()));
    qDebug() << __FUNCTION__ << cTypeMap.key(ui->SampleType->itemData(val, Qt::DisplayRole).toString());
    emit ChangeAudioConnection(m_DevInfo, m_FormatAudio);
}

QAudioDeviceInfo AudioSourceConfig::DevInfo() const
{
    return m_DevInfo;
}
