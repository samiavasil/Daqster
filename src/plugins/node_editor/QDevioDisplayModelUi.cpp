#include "QDevioDisplayModelUi.h"
#include "ui_QDevioDisplayModelUi.h"
#include <QtCharts/QChartView>
#include <QtCharts/QLineSeries>
#include <QtCharts/QChart>
#include <QtCharts/QValueAxis>
#include <QtWidgets/QVBoxLayout>


QT_CHARTS_USE_NAMESPACE

QDevioDisplayModelUi::QDevioDisplayModelUi(QLineSeries *series, QWidget *parent) :
    QWidget(parent),
    ui(new Ui::QDevioDisplayModelUi),
    m_series(series),
    m_chart(new QChart)
{
    ui->setupUi(this);

    QChartView *chartView = new QChartView(m_chart);
    chartView->setMinimumSize(400, 200);
    m_chart->addSeries(m_series);
    QValueAxis *axisX = new QValueAxis;
    axisX->setRange(0, 8000);
    axisX->setLabelFormat("%g");
    axisX->setTitleText("Samples");
    QValueAxis *axisY = new QValueAxis;
    axisY->setRange(-1.3, 1.3);
    axisY->setTitleText("Audio level");
    QLineSeries* ser = new QLineSeries();
    for(int i=0;i<2000;i++){
        series->append(i,0.5);
        ser->append(i,0.2);
    }
    m_chart->addSeries(ser);
    m_chart->setAxisX(axisX, ser);
    m_chart->setAxisY(axisY, ser);

    m_chart->setAxisX(axisX, m_series);
    m_chart->setAxisY(axisY, m_series);
    m_chart->legend()->hide();
    m_chart->setTitle("Data from the microphone");

    QVBoxLayout *mainLayout = new QVBoxLayout;
    mainLayout->addWidget(chartView);
    setLayout(mainLayout);


}

QDevioDisplayModelUi::~QDevioDisplayModelUi()
{
    m_chart->removeSeries(m_series);
    delete ui;
}
