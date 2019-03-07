#include "QDevioDisplayModelUi.h"
#include "ui_QDevioDisplayModelUi.h"
#include <QtCharts/QChartView>
#include <QtCharts/QLineSeries>
#include <QtCharts/QChart>
#include <QtCharts/QValueAxis>
#include <QtWidgets/QVBoxLayout>


QT_CHARTS_USE_NAMESPACE

QDevioDisplayModelUi::QDevioDisplayModelUi(QLineSeries *series, QWidget *parent) :
    m_series(series),
    m_chart(new QChart),
    QWidget(parent),
    ui(new Ui::QDevioDisplayModelUi)
{
    ui->setupUi(this);

    QChartView *chartView = new QChartView(m_chart);
    chartView->setMinimumSize(800, 600);
    m_chart->addSeries(m_series);
    QValueAxis *axisX = new QValueAxis;
    axisX->setRange(0, 2000);
    axisX->setLabelFormat("%g");
    axisX->setTitleText("Samples");
    QValueAxis *axisY = new QValueAxis;
    axisY->setRange(-1, 1);
    axisY->setTitleText("Audio level");
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
