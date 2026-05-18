#include "AudioComboModel.h"

#include "AudioCompat.h"

#include <QMap>

// Display strings for enum types
static const QMap<AudioCompat::SampleType, QString> kTypeMap{
    {AudioCompat::Unknown,     "Unknown"},
    {AudioCompat::SignedInt,   "SignedInt"},
    {AudioCompat::UnSignedInt, "UnSignedInt"},
    {AudioCompat::Float,       "Float"}
};

static const QMap<AudioCompat::Endian, QString> kEndianMap{
    {AudioCompat::LittleEndian, "LittleEndian"},
    {AudioCompat::BigEndian,    "BigEndian"},
};

// ---------------------------------------------------------------------------

QAudioComboModel::QAudioComboModel(std::function<QAudioFormat()>           currentFormat,
                                   std::function<bool(const QAudioFormat&)> isSupported,
                                   AudioModelType                           type,
                                   QObject*                                 parent)
    : QAbstractListModel(parent)
    , m_CurrentFormat(std::move(currentFormat))
    , m_IsSupported(std::move(isSupported))
    , m_Type(type)
{
}

void QAudioComboModel::populate(const QList<QVariant>& data)
{
    beginResetModel();
    m_Data = data;
    endResetModel();
}

// ---------------------------------------------------------------------------

int QAudioComboModel::rowCount(const QModelIndex& parent) const
{
    if (parent.isValid())
        return 0;
    return m_Data.count();
}

QVariant QAudioComboModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() >= m_Data.count())
        return QVariant();

    if (role == Qt::DisplayRole || role == Qt::WhatsThisRole) {
        switch (m_Type) {
        case BYTES_ORDER:
            return kEndianMap.value(
                static_cast<AudioCompat::Endian>(m_Data[index.row()].toInt()),
                "Unknown");
        case SAMPLE_TYPE:
            return kTypeMap.value(
                static_cast<AudioCompat::SampleType>(m_Data[index.row()].toInt()),
                "Unknown");
        default:
            return m_Data[index.row()];
        }
    }

    // Qt::UserRole always returns the raw stored value
    if (role == Qt::UserRole)
        return m_Data[index.row()];

    return QVariant();
}

Qt::ItemFlags QAudioComboModel::flags(const QModelIndex& index) const
{
    if (!index.isValid() || index.row() >= m_Data.count())
        return Qt::NoItemFlags;

    QAudioFormat testFormat = makeTestFormat(index.row());

    return m_IsSupported(testFormat)
               ? (Qt::ItemIsEnabled | Qt::ItemIsSelectable)
               : Qt::NoItemFlags;
}

// ---------------------------------------------------------------------------

QAudioFormat QAudioComboModel::makeTestFormat(int row) const
{
    // Start from the current format so all other fields keep their values.
    // Only override the field this model represents, then test support.
    QAudioFormat f = m_CurrentFormat();

    switch (m_Type) {
    case CHANNEL_NUMBER:
        f.setChannelCount(m_Data[row].toInt());
        break;
    case CODEC:
        AudioCompat::setCodec(f, m_Data[row].toString());
        break;
    case BYTES_ORDER:
        AudioCompat::setByteOrder(f, static_cast<AudioCompat::Endian>(m_Data[row].toInt()));
        break;
    case SAMPLE_RATE:
        f.setSampleRate(m_Data[row].toInt());
        break;
    case SAMPLE_SIZE:
        AudioCompat::setSampleSize(f, m_Data[row].toInt());
        break;
    case SAMPLE_TYPE:
        AudioCompat::setSampleType(f, static_cast<AudioCompat::SampleType>(m_Data[row].toInt()));
        break;
    }

    return f;
}
