#ifndef QDEVIODISPLAYMODELUI_H
#define QDEVIODISPLAYMODELUI_H

#include <QWidget>
#include <QtCharts/QChartGlobal>
namespace Ui {
class QDevioDisplayModelUi;
}
class XYSeriesIODevice;

QT_CHARTS_BEGIN_NAMESPACE
class QLineSeries;
class QChart;
QT_CHARTS_END_NAMESPACE

QT_CHARTS_USE_NAMESPACE

class QDevioDisplayModelUi : public QWidget
{
    Q_OBJECT

public:
    explicit QDevioDisplayModelUi(QLineSeries* series,  QWidget *parent = 0);
    ~QDevioDisplayModelUi();

private:
    Ui::QDevioDisplayModelUi *ui;
    QLineSeries* m_series;
    QChart* m_chart;
public slots:
    void bufferReady(QVector<QPointF>& buff, int channel);
};

#endif // QDEVIODISPLAYMODELUI_H
