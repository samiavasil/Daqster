#ifndef AUDIOCOMBOMODEL_H
#define AUDIOCOMBOMODEL_H

#include <QAbstractListModel>
#include <QtMultimedia/QAudioFormat>
#include <functional>

/**
 * @brief List model for QComboBox widgets in AudioSourceConfig.
 *
 * Stores raw device-capability values (int for enums, int/QString for others).
 * populate() is called by AudioSourceConfig::InitAudioParams every time the
 * selected audio device changes.
 *
 * flags() checks whether each item forms a supported format
 * (given the OTHER fields currently selected) and disables unsupported ones.
 */
class QAudioComboModel : public QAbstractListModel
{
    Q_OBJECT

public:
    enum AudioModelType {
        CHANNEL_NUMBER,
        CODEC,
        BYTES_ORDER,
        SAMPLE_RATE,
        SAMPLE_SIZE,
        SAMPLE_TYPE
    };

    /**
     * @param currentFormat  Callable that returns the currently active QAudioFormat.
     *                       Used by flags() to build the test-format for every item.
     * @param isSupported    Callable that tests whether a QAudioFormat is supported
     *                       by the currently selected device.
     * @param type           Which audio parameter this model represents.
     */
    explicit QAudioComboModel(std::function<QAudioFormat()>           currentFormat,
                               std::function<bool(const QAudioFormat&)> isSupported,
                               AudioModelType                           type,
                               QObject*                                 parent = nullptr);

    /**
     * @brief Replace model contents. Triggers a full view reset.
     *
     * For CHANNEL_NUMBER, SAMPLE_RATE, SAMPLE_SIZE: store int values.
     * For CODEC:                                     store QString values.
     * For BYTES_ORDER:  store static_cast<int>(QAudioFormat::Endian).
     * For SAMPLE_TYPE:  store static_cast<int>(QAudioFormat::SampleType).
     */
    void populate(const QList<QVariant>& data);

    // --- QAbstractListModel interface ---
    int      rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    Qt::ItemFlags flags(const QModelIndex& index) const override;

private:
    /// Builds a test QAudioFormat by taking the current format and overriding
    /// exactly the one field this model controls with the value at @p row.
    QAudioFormat makeTestFormat(int row) const;

    std::function<QAudioFormat()>           m_CurrentFormat;
    std::function<bool(const QAudioFormat&)> m_IsSupported;
    AudioModelType                           m_Type;
    QList<QVariant>                          m_Data;
};

#endif // AUDIOCOMBOMODEL_H
