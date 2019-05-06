#ifndef QDEVIODISPLAYMODELUI_H
#define QDEVIODISPLAYMODELUI_H

#include <QWidget>

#include <QtCharts/QChartGlobal>

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
 //TODO   virtual disp_hndl_t AddChart();
 //   virtual void AddSeries(disp_hndl_t chart, int num);
 //   virtual void RemoveSeria(disp_hndl_t chart, int idx);

private:
    Ui::QDevioDisplayModelUi *ui;
    QLineSeries* m_series;
    QChart* m_chart;
public slots:
    void bufferReady(QVector<QPointF>& buff, int channel);
protected slots:
    void pollData();
};

#endif // QDEVIODISPLAYMODELUI_H
