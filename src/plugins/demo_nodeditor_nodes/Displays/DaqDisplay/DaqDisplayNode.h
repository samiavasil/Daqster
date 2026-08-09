#ifndef DAQDISPLAYNODE_H
#define DAQDISPLAYNODE_H

#include "SampledData.h"

#include "QtChartsCompat.h"

#include <QtNodes/NodeDelegateModel>

#include <QVector>

#include <functional>
#include <memory>

class QComboBox;
class QLabel;
class QStackedWidget;
class QWidget;

/**
 * @brief DAQ Display node — DataPlot architecture (REQ-SW-PL-022 §5).
 *
 * Generalizes the legacy AudioDisplayModel: visualizes ANY sampled source
 * (audio, DAQ, sensors) — waveform AND FFT through the same node. Consumes
 * SampledData ("sample") via setInData (NodeData flow, NOT QDevIO bytes).
 * The unified SampledData type + `domain` discriminator make the node
 * domain-agnostic: "audio", "vibration", "daq", "ecg", ... all render without
 * node changes (AC 7).
 *
 * Architecture: the node hosts a series of PLOT SLOTS. Each slot is
 *   { channel selection, preprocessing function, plot view }.
 * The preprocessing function is the JIT-ready extension point: it is a
 * std::function<QVector<float>(const SampledData&)> so a future scriptable /
 * compiled function can plug in without node changes. v1 built-ins:
 * identity (time-domain waveform) and FFT (magnitude spectrum).
 *
 * Rendering is REAL Qt Charts (QtChartsCompat) — replaces the old stubs in
 * GenericDisplayNode.cpp and AudioDisplayModel.cpp (AC 6).
 */
class DaqDisplayNode : public QtNodes::NodeDelegateModel
{
    Q_OBJECT

public:
    DaqDisplayNode();
    ~DaqDisplayNode() override;

    QString caption() const override
    { return QStringLiteral("DAQ Display"); }

    bool captionVisible() const override
    { return false; }

    QString name() const override
    { return QStringLiteral("DaqDisplay"); }

    QJsonObject save() const override;

    unsigned int nPorts(QtNodes::PortType portType) const override;

    QtNodes::NodeDataType dataType(QtNodes::PortType portType,
                                   QtNodes::PortIndex portIndex) const override;

    std::shared_ptr<QtNodes::NodeData> outData(QtNodes::PortIndex port) override;

    void setInData(std::shared_ptr<QtNodes::NodeData> data,
                   QtNodes::PortIndex portIndex) override;

    QWidget *embeddedWidget() override;

    /// Built-in preprocessing functions (v1, JIT-ready slot interface).
    static QVector<float> channelSamples(const SampledData &data, int channel);
    static QVector<float> spectrumSamples(const SampledData &data, int channel);

private:
    using PreprocessFn = std::function<QVector<float>(const SampledData &)>;

    /// One DataPlot slot: {channel selection, preprocessing fn, plot view}.
    struct PlotSlot {
        QString title;
        int channelIndex = 0;
        bool frequencyAxis = false;       // false → time (seconds), true → Hz
        PreprocessFn preprocess;          // JIT-ready extension point
        QtChartsCompat::LineSeries *series = nullptr;
        QtChartsCompat::Chart *chart = nullptr;
        QtChartsCompat::ValueAxis *axisX = nullptr;
        QtChartsCompat::ValueAxis *axisY = nullptr;
    };

    void setupUi();
    void addSlot(const QString &title, bool frequencyAxis,
                 int channelIndex, PreprocessFn preprocess);
    void refresh();
    void updateSlot(int slotIndex);
    void bindBuiltinPreprocessors(int channelIndex);

private slots:
    void onChannelChanged(int index);

private:
    QWidget *m_root = nullptr;
    QLabel *m_domainLabel = nullptr;
    QComboBox *m_channelCombo = nullptr;
    QStackedWidget *m_stack = nullptr;
    int m_timeChartIndex = -1;
    int m_fftChartIndex = -1;

    QVector<PlotSlot> m_slots;
    std::shared_ptr<SampledData> m_lastData;
    int m_currentChannel = 0;
    bool m_updating = false;
};

#endif // DAQDISPLAYNODE_H