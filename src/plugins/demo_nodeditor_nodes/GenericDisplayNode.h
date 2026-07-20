#ifndef GENERICDISPLAYNODE_H
#define GENERICDISPLAYNODE_H

#include <QtNodes/NodeDelegateModel>
#include <QVector>
#include <QMap>

class QStackedWidget;

// Include full definition — ChartView inherits QWidget, needed for addWidget()
#include "QtChartsCompat.h"

/**
 * @brief Display node for generic_numeric port type.
 *
 * Handles typed numeric data (int16/uint16/int32/uint32/float32/float64)
 * with manual configuration: channels, SampleType, sampleRate, scale.
 *
 * Views: TimeChart, FFTSpectrum, SensorGauge (future).
 */
class GenericDisplayNode : public QtNodes::NodeDelegateModel
{
    Q_OBJECT

public:
    GenericDisplayNode();
    ~GenericDisplayNode() override;

    QString caption() const override
    { return QStringLiteral("Generic Display"); }

    bool captionVisible() const override
    { return false; }

    QString name() const override
    { return QStringLiteral("GenericDisplay"); }

    QJsonObject save() const override;

    unsigned int nPorts(QtNodes::PortType portType) const override;

    QtNodes::NodeDataType dataType(QtNodes::PortType portType,
                                   QtNodes::PortIndex portIndex) const override;

    std::shared_ptr<QtNodes::NodeData> outData(QtNodes::PortIndex const port) override;

    void setInData(std::shared_ptr<QtNodes::NodeData> data,
                   QtNodes::PortIndex const portIndex) override;

    QWidget* embeddedWidget() override;

private:
    void setupUi();
    void updateTimeChart(const QVector<QVector<double>>& channels);
    void updateFFTChart(const QVector<QVector<double>>& channels);

    QStackedWidget* m_stack = nullptr;
    QtChartsCompat::ChartView* m_timeChart = nullptr;
    QtChartsCompat::ChartView* m_fftChart = nullptr;
    QVector<QtChartsCompat::LineSeries*> m_timeSeries;
    QVector<QtChartsCompat::LineSeries*> m_fftSeries;
    int m_configPanelIndex = -1;
    int m_timeChartIndex = -1;
    int m_fftChartIndex = -1;
};

#endif // GENERICDISPLAYNODE_H
