#ifndef QDEVIODISPLAYMODELUI_H
#define QDEVIODISPLAYMODELUI_H

#include <QWidget>
#include <QtCharts/QChartGlobal>
#include <QMap>
#include <QVector>

namespace Ui {
class QDevioDisplayModelUi;
}


QT_CHARTS_BEGIN_NAMESPACE
class QLineSeries;
class QChart;
QT_CHARTS_END_NAMESPACE

QT_CHARTS_USE_NAMESPACE

class QDevioDisplayModelUi : public QWidget
{
    Q_OBJECT

public:
    typedef int disp_hndl_t;
    explicit QDevioDisplayModelUi(QWidget *parent = nullptr);
    ~QDevioDisplayModelUi();
    void UpdateConfig();
    virtual disp_hndl_t AddChart();
    virtual void RemoveChart(disp_hndl_t hndl);
    virtual int SetSeries(disp_hndl_t chart, int num);
    virtual int RemoveSeries();

private:
    Ui::QDevioDisplayModelUi *ui;
    QVector<QLineSeries*> m_series;
    QMap<disp_hndl_t, QVector<QLineSeries*>> m_SeriesMap;
    QMap<disp_hndl_t, QChart*> m_ChartMap;
    QChart* m_chart;
    static disp_hndl_t m_NextHndl;
public slots:
    void bufferReady(QVector<QPointF>& buff, int channel);
protected slots:
    void pollData();
};

#endif // QDEVIODISPLAYMODELUI_H
