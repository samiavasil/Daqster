#include "QDevioDisplayModelUi.h"
#include "ui_QDevioDisplayModelUi.h"
#include <QtCharts/QChartView>
#include <QtCharts/QLineSeries>
#include <QtCharts/QChart>
#include <QtCharts/QValueAxis>
#include <QtWidgets/QVBoxLayout>
#include <QTimer>

QT_CHARTS_USE_NAMESPACE
QDevioDisplayModelUi::disp_hndl_t QDevioDisplayModelUi::m_NextHndl;

QDevioDisplayModelUi::QDevioDisplayModelUi(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::QDevioDisplayModelUi),
    m_chart(new QChart)
{
    m_series.append(new QLineSeries);
    m_series.append(new QLineSeries);
    ui->setupUi(this);

    QChartView *chartView = new QChartView(m_chart);
    chartView->setMinimumSize(400, 200);
    QValueAxis *axisX = new QValueAxis;
    axisX->setRange(0, 8000);
    axisX->setLabelFormat("%g");
    axisX->setTitleText("Samples");
    QValueAxis *axisY = new QValueAxis;
    axisY->setRange(-1.3, 1.3);
    axisY->setTitleText("Audio level");

    for(int j = 0; j < 2; j++) {
        for(int i=0;i<2000;i++){
            m_series[j]->append(i,0.5*j);
        }
        m_chart->addSeries(m_series[j]);
        m_chart->setAxisX(axisX, m_series[j]);
        m_chart->setAxisY(axisY, m_series[j]);
    }
    m_chart->legend()->hide();
    m_chart->setTitle("Data from the microphone");

    ui->gridLayout->addWidget(chartView);
    QTimer::singleShot(100, this, SLOT(pollData()));
}

QDevioDisplayModelUi::~QDevioDisplayModelUi()
{
//TBD ???Is this needed?
    while( m_series.count() ) {
        m_chart->removeSeries(m_series[0]);
        delete m_series[0];
        m_series.removeFirst();
    }

    delete ui;
}

QDevioDisplayModelUi::disp_hndl_t QDevioDisplayModelUi::AddChart()
{
    disp_hndl_t hndl = m_NextHndl;
    m_ChartMap[hndl] = new QChart;
    m_NextHndl++;
    return hndl;
}

void QDevioDisplayModelUi::RemoveChart(QDevioDisplayModelUi::disp_hndl_t hndl)
{
    QVector<QLineSeries*> series = m_SeriesMap.take(hndl);

    while(series.count()) {
        m_chart->removeSeries(series[0]);
        delete series[0];
        series.removeFirst();
    }
}

void RemoveSeriesFromVector(QVector<QLineSeries*>& series) {
    /*while(series.count()) {
        m_chart->removeSeries(series[0]);
        delete series[0];
        series.removeFirst();
    }*/
}

int QDevioDisplayModelUi::SetSeries(QDevioDisplayModelUi::disp_hndl_t chart, int num)
{
    //m_SeriesMap
}

int QDevioDisplayModelUi::RemoveSeries()
{

}

void QDevioDisplayModelUi::pollData() {

}

void QDevioDisplayModelUi::bufferReady(QVector<QPointF> &buff, int channel)
{
    m_series[channel]->replace(buff);
}
