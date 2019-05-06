#include "QDevioDisplayModelUi.h"
#include "ui_QDevioDisplayModelUi.h"
#include <QtCharts/QChartView>
#include <QtCharts/QLineSeries>
#include <QtCharts/QChart>
#include <QtCharts/QValueAxis>
#include <QtWidgets/QVBoxLayout>
#include <QTimer>

QT_CHARTS_USE_NAMESPACE

QDevioDisplayModelUi::QDevioDisplayModelUi(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::QDevioDisplayModelUi),
    m_series(new QLineSeries[2]),
    m_chart(new QChart)
{
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
            m_series[j].append(i,0.5*j);
        }
        m_chart->addSeries(&m_series[j]);
        m_chart->setAxisX(axisX, &m_series[j]);
        m_chart->setAxisY(axisY, &m_series[j]);
    }
    m_chart->legend()->hide();
    m_chart->setTitle("Data from the microphone");

    QVBoxLayout *mainLayout = new QVBoxLayout;
    mainLayout->addWidget(chartView);
    setLayout(mainLayout);
    QTimer::singleShot(100, this, SLOT(pollData()));

}

QDevioDisplayModelUi::~QDevioDisplayModelUi()
{
    for(int j = 0; j < 2; j++) {
        m_chart->removeSeries(&m_series[j]);
     //   m_series[j].deleteLater();
    }

    delete ui;
}

void QDevioDisplayModelUi::pollData() {

}

void QDevioDisplayModelUi::bufferReady(QVector<QPointF> &buff, int channel)
{
    m_series[channel].replace(buff);
}
