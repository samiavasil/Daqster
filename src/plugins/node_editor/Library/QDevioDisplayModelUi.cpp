#include "QDevioDisplayModelUi.h"
#include "ui_QDevioDisplayModelUi.h"
#include <QtCharts/QChartView>
#include <QtCharts/QLineSeries>
#include <QtCharts/QChart>
#include <QtCharts/QValueAxis>
#include <QtWidgets/QVBoxLayout>
#include <QTimer>
#include<QMenu>

QT_CHARTS_USE_NAMESPACE

QDevioDisplayModelUi::QDevioDisplayModelUi(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::QDevioDisplayModelUi),
    m_NextHndl(0)
  /*,
            m_chart(new QChart)*/
{
    m_GridType = LIST;
    m_ColCount = 1;
    ui->setupUi(this);
    ui->chartControl->setVisible(false);
    populateThemeBox();
    populateAnimationBox();
    populateLegendBox();
    ui->antialiasing->setChecked(true);
#if 0
    m_series.append(new QLineSeries);
    m_series.append(new QLineSeries);
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
#endif
    connect(ui->animation, SIGNAL(currentIndexChanged(int)), this, SLOT(updateUI()));
    connect(ui->legend, SIGNAL(currentIndexChanged(int)), this, SLOT(updateUI()));
    connect(ui->theme, SIGNAL(currentIndexChanged(int)), this, SLOT(updateUI()));
    connect(ui->antialiasing, SIGNAL(toggled(bool)), this, SLOT(updateUI()));
    connect(ui->gridColumns, SIGNAL(valueChanged(int)), this, SLOT(gridChanged(int)));
    disp_hndl_t hndl = AddChart();//TODO: Fix Me
    hndl = AddChart();
     hndl = AddChart();
//    SetSeries(hndl, 5);
    QTimer::singleShot(100, this, SLOT(pollData()));
}

QDevioDisplayModelUi::~QDevioDisplayModelUi()
{
    //TBD ???Is this needed?
#if 0
    while( m_series.count() ) {
        m_chart->removeSeries(m_series[0]);
        delete m_series[0];
        m_series.removeFirst();
    }
#endif
    delete ui;
}

QDevioDisplayModelUi::disp_hndl_t QDevioDisplayModelUi::AddChart()
{
    disp_hndl_t hndl = m_NextHndl;
    m_ChartMap[hndl] = new QChartView(new QChart);
    m_ChartMap[hndl]->setMinimumSize(400, 200);
    ui->gridLayout->addWidget(m_ChartMap[hndl]);
    m_NextHndl++;
    return hndl;
}

void QDevioDisplayModelUi::RemoveChart(QDevioDisplayModelUi::disp_hndl_t hndl)
{
#if 0
    QVector<QLineSeries*> series = m_SeriesMap.take(hndl);

    while(series.count()) {
        m_chart->removeSeries(series[0]);
        delete series[0];
        series.removeFirst();
    }
#endif
}

void RemoveSeriesFromVector(QVector<QLineSeries*>& series) {
    /*while(series.count()) {
        m_chart->removeSeries(series[0]);
        delete series[0];
        series.removeFirst();
    }*/ QValueAxis *axisX = new QValueAxis;
}

void QDevioDisplayModelUi::updateUI()
{
    QChart::ChartTheme theme = static_cast<QChart::ChartTheme>(
                ui->theme->itemData(ui->theme->currentIndex()).toInt());

    if (!m_ChartMap.isEmpty() && m_ChartMap.first()->chart()->theme() != theme) {
        for (QChartView *chartView : m_ChartMap) {
            chartView->chart()->setTheme(theme);
        }

        // Set palette colors based on selected theme
        QPalette pal = window()->palette();
        if (theme == QChart::ChartThemeLight) {
            pal.setColor(QPalette::Window, QRgb(0xf0f0f0));
            pal.setColor(QPalette::WindowText, QRgb(0x404044));
            //![8]
        } else if (theme == QChart::ChartThemeDark) {
            pal.setColor(QPalette::Window, QRgb(0x121218));
            pal.setColor(QPalette::WindowText, QRgb(0xd6d6d6));
        } else if (theme == QChart::ChartThemeBlueCerulean) {
            pal.setColor(QPalette::Window, QRgb(0x40434a));
            pal.setColor(QPalette::WindowText, QRgb(0xd6d6d6));
        } else if (theme == QChart::ChartThemeBrownSand) {
            pal.setColor(QPalette::Window, QRgb(0x9e8965));
            pal.setColor(QPalette::WindowText, QRgb(0x404044));
        } else if (theme == QChart::ChartThemeBlueNcs) {
            pal.setColor(QPalette::Window, QRgb(0x018bba));
            pal.setColor(QPalette::WindowText, QRgb(0x404044));
        } else if (theme == QChart::ChartThemeHighContrast) {
            pal.setColor(QPalette::Window, QRgb(0xffab03));
            pal.setColor(QPalette::WindowText, QRgb(0x181818));
        } else if (theme == QChart::ChartThemeBlueIcy) {
            pal.setColor(QPalette::Window, QRgb(0xcee7f0));
            pal.setColor(QPalette::WindowText, QRgb(0x404044));
        } else {
            pal.setColor(QPalette::Window, QRgb(0xf0f0f0));
            pal.setColor(QPalette::WindowText, QRgb(0x404044));
        }
        window()->setPalette(pal);
    }

    // Update antialiasing
    bool checked = ui->antialiasing->isChecked();
    for (QChartView *chart : m_ChartMap)
        chart->setRenderHint(QPainter::Antialiasing, checked);

    // Update animation options
    QChart::AnimationOptions options(
                ui->animation->itemData(ui->animation->currentIndex()).toInt());
    if (!m_ChartMap.isEmpty() && m_ChartMap.first()->chart()->animationOptions() != options) {
        for (QChartView *chartView : m_ChartMap)
            chartView->chart()->setAnimationOptions(options);
    }

    // Update legend alignment
    Qt::Alignment alignment(
                ui->legend->itemData(ui->legend->currentIndex()).toInt());

    if (!alignment) {
        for (QChartView *chartView : m_ChartMap)
            chartView->chart()->legend()->hide();
    } else {
        for (QChartView *chartView : m_ChartMap) {
            chartView->chart()->legend()->setAlignment(alignment);
            chartView->chart()->legend()->show();
        }
    }

}

void QDevioDisplayModelUi::populateThemeBox()
{
    // add items to theme combobox
    ui->theme->addItem("Light", QChart::ChartThemeLight);
    ui->theme->addItem("Blue Cerulean", QChart::ChartThemeBlueCerulean);
    ui->theme->addItem("Dark", QChart::ChartThemeDark);
    ui->theme->addItem("Brown Sand", QChart::ChartThemeBrownSand);
    ui->theme->addItem("Blue NCS", QChart::ChartThemeBlueNcs);
    ui->theme->addItem("High Contrast", QChart::ChartThemeHighContrast);
    ui->theme->addItem("Blue Icy", QChart::ChartThemeBlueIcy);
    ui->theme->addItem("Qt", QChart::ChartThemeQt);
}

void QDevioDisplayModelUi::populateAnimationBox()
{
    // add items to animation combobox
    ui->animation->addItem("No Animations", QChart::NoAnimation);
    ui->animation->addItem("GridAxis Animations", QChart::GridAxisAnimations);
    ui->animation->addItem("Series Animations", QChart::SeriesAnimations);
    ui->animation->addItem("All Animations", QChart::AllAnimations);
}

void QDevioDisplayModelUi::populateLegendBox()
{
    // add items to legend combobox
    ui->legend->addItem("No Legend ", 0);
    ui->legend->addItem("Legend Top", Qt::AlignTop);
    ui->legend->addItem("Legend Bottom", Qt::AlignBottom);
    ui->legend->addItem("Legend Left", Qt::AlignLeft);
    ui->legend->addItem("Legend Right", Qt::AlignRight);
    ui->legend->setCurrentIndex(0);
}
#include<QDebug>
void QDevioDisplayModelUi::changeGrid(grid_type type) {

    if( m_GridType != type) {
        m_GridType = type;
    while(auto item = ui->gridLayout->itemAt(0)) {
        ui->gridLayout->removeItem(item);
        delete item;
    }
qDebug() << "Col count: " << ui->gridLayout->columnCount();
    if(type == LIST) {
        int i = 0;
        for (QChartView *chartView : m_ChartMap) {
            ui->gridLayout->addWidget(chartView, i, 0);
            i++;
        }
    } else {
        int i = 0;
        for (QChartView *chartView : m_ChartMap) {
            ui->gridLayout->addWidget(chartView, i/m_ColCount, i%m_ColCount);
            i++;
        }
    }
    }
}

void QDevioDisplayModelUi::gridChanged(int val)
{
    m_ColCount = val;
    changeGrid(m_GridType);
}

int QDevioDisplayModelUi::SetSeries(QDevioDisplayModelUi::disp_hndl_t hndl, int num)
{
    int Ret = -1;

    auto chartView = m_ChartMap.value(hndl, nullptr);

    if(chartView) {
        auto chart = chartView->chart();
        auto series =  m_SeriesMap.value(hndl, nullptr);

        if(series) {
            while(series->count()) {
                auto first = series->first();
                chart->removeSeries(first);
                delete first;
                series->removeFirst();
            }
        }else {
            series = new QVector<QLineSeries*>;
            m_SeriesMap[hndl] = series;
        }
        QValueAxis *axisX = new QValueAxis;
        axisX->setRange(0, 8000);
        axisX->setLabelFormat("%g");
        axisX->setTitleText("Samples");
        QValueAxis *axisY = new QValueAxis;


        axisY->setRange(-3, 3);
        axisY->setTitleText("Audio level");
        for(int j = 0; j < num; j++) {
            auto seria = new QLineSeries;
            seria->setName(QString("Seria %1").arg(j));

            series->append(seria);
            for(int i=0;i<8000;i++){
                series->value(j)->append(i, -1 + 0.5*j);
            }
            chart->addSeries(series->value(j));

            //     chart->setAxisX(axisX, series->value(j));
            //       chart->setAxisY(axisY, series->value(j));
        }

        chart->createDefaultAxes();

        //chart->legend()->hide();
        chart->setTitle("Data from the microphone");

        // Update legend alignment
        Qt::Alignment alignment(
                    ui->legend->itemData(ui->legend->currentIndex()).toInt());
        chart->axisX()->setRange(0, 8000);
        chart->axisY() ->setRange(-3, 3);
        chart->legend()->setAlignment(Qt::AlignLeft);

        if (!alignment) {
            for (QChartView *chartView : m_ChartMap)
                chartView->chart()->legend()->hide();
        } else {
            for (QChartView *chartView : m_ChartMap) {
                chartView->chart()->legend()->setAlignment(alignment);
                chartView->chart()->legend()->show();
            }
        }

        Ret = num;
    }

    return Ret;
}

int QDevioDisplayModelUi::RemoveSeries()
{

}

void QDevioDisplayModelUi::contextMenuEvent(QContextMenuEvent *event)
{

    QMenu menu;
    QAction *showAction    = menu.addAction("Show Chart Control");
    showAction->setCheckable(true);
    showAction->setChecked(ui->chartControl->isVisible());
    QMenu* menu_ = menu.addMenu("View");
    QAction *alist = menu_->addAction("List");
    QAction *agrid = menu_->addAction("Grid");
    QAction *selectedAction = menu.exec( event->globalPos() );

    if(alist == selectedAction) {
        changeGrid(LIST);
    } else if(agrid == selectedAction) {
        changeGrid(GRID);
    } else{
        ui->chartControl->setVisible(selectedAction->isChecked());
    }

}

void QDevioDisplayModelUi::pollData() {

}

void QDevioDisplayModelUi::bufferReady(QVector<QPointF> &buff, int channel)
{
    auto series = m_SeriesMap.value(0, NULL);//TODO Fix this 0 index
    Q_ASSERT(series != NULL);
    auto seria = series->value(channel);
    if(seria != nullptr)
        seria->replace(buff);
}
