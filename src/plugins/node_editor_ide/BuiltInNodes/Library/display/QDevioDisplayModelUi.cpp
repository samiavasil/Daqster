#include "QDevioDisplayModelUi.h"
#include "ui_QDevioDisplayModelUi.h"
#include <QtCharts/QChartView>
#include <QtCharts/QLineSeries>
#include <QtCharts/QChart>
#include <QtCharts/QValueAxis>
#include <QtWidgets/QVBoxLayout>
#include <QTimer>
#include <QMenu>
#include <QtMath>
#include <cmath>
#include <algorithm>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif


static int largestPowerOfTwoLe(int n)
{
    int p = 1;
    while (p * 2 <= n)
        p *= 2;
    return p;
}

static void applyHannWindow(QVector<double> &data)
{
    const int n = data.size();
    if (n < 2) return;
    for (int i = 0; i < n; ++i) {
        const double hann = 0.5 * (1.0 - std::cos(2.0 * M_PI * i / (n - 1)));
        data[i] *= hann;
    }
}

QDevioDisplayModelUi::QDevioDisplayModelUi(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::QDevioDisplayModelUi),
    m_NextHndl(0)
{
    m_ColCount = 1;
    ui->setupUi(this);
    ui->chartControl->setVisible(false);
    populateThemeBox();
    populateAnimationBox();
    populateLegendBox();
    ui->antialiasing->setChecked(true);
    connect(ui->animation, SIGNAL(currentIndexChanged(int)), this, SLOT(updateUI()));
    connect(ui->legend, SIGNAL(currentIndexChanged(int)), this, SLOT(updateUI()));
    connect(ui->theme, SIGNAL(currentIndexChanged(int)), this, SLOT(updateUI()));
    connect(ui->antialiasing, SIGNAL(toggled(bool)), this, SLOT(updateUI()));
    connect(ui->gridColumns, SIGNAL(valueChanged(int)), this, SLOT(gridChanged(int)));

    const disp_hndl_t hndl = AddChart();

    // FFT chart — hidden by default
    m_fftHandle = AddChart();
    auto *fftChartView = m_ChartMap[m_fftHandle];
    auto *fftChart = fftChartView->chart();
    fftChart->setTitle(tr("Frequency Spectrum"));
    fftChart->legend()->hide();

    auto *fftSeries = new QtChartsCompat::LineSeries();
    fftSeries->setName(tr("Magnitude"));
    fftChart->addSeries(fftSeries);

    auto *axisX = new QtChartsCompat::ValueAxis();
    axisX->setTitleText(tr("Frequency Bin"));
    axisX->setLabelFormat("%d");
    axisX->setRange(0, 4096);

    auto *axisY = new QtChartsCompat::ValueAxis();
    axisY->setTitleText(tr("Magnitude"));
    axisY->setLabelFormat("%.3f");
    axisY->setRange(0, 1);

    fftChart->addAxis(axisX, Qt::AlignBottom);
    fftChart->addAxis(axisY, Qt::AlignLeft);
    fftSeries->attachAxis(axisX);
    fftSeries->attachAxis(axisY);

    // Make a placeholder series entry so SetSeries / RemoveSeries don't break
    auto *seriesVec = new QVector<QtChartsCompat::LineSeries*>();
    seriesVec->append(fftSeries);
    m_SeriesMap[m_fftHandle] = seriesVec;

    fftChartView->setVisible(false);

    QTimer::singleShot(100, this, SLOT(pollData()));
}

QDevioDisplayModelUi::~QDevioDisplayModelUi()
{
}

QDevioDisplayModelUi::disp_hndl_t QDevioDisplayModelUi::AddChart()
{
    disp_hndl_t hndl = m_NextHndl;
    m_ChartMap[hndl] = new QtChartsCompat::ChartView(new QtChartsCompat::Chart);
    m_ChartMap[hndl]->setMinimumSize(400, 200);
    ui->gridLayout->addWidget(m_ChartMap[hndl]);
    m_NextHndl++;
    return hndl;
}

void QDevioDisplayModelUi::RemoveChart(QDevioDisplayModelUi::disp_hndl_t hndl)
{
}

void RemoveSeriesFromVector(QVector<QtChartsCompat::LineSeries*>& series) {
 QtChartsCompat::ValueAxis *axisX = new QtChartsCompat::ValueAxis;
}

void QDevioDisplayModelUi::updateUI()
{
    QtChartsCompat::Chart::ChartTheme theme = static_cast<QtChartsCompat::Chart::ChartTheme>(
        ui->theme->itemData(ui->theme->currentIndex()).toInt());

    if (!m_ChartMap.isEmpty() && m_ChartMap.first()->chart()->theme() != theme) {
        for (QtChartsCompat::ChartView *chartView : m_ChartMap) {
            chartView->chart()->setTheme(theme);
        }

        // Set palette colors based on selected theme
        QPalette pal = window()->palette();
        if (theme == QtChartsCompat::Chart::ChartThemeLight) {
            pal.setColor(QPalette::Window, QRgb(0xf0f0f0));
            pal.setColor(QPalette::WindowText, QRgb(0x404044));
            //![8]
        } else if (theme == QtChartsCompat::Chart::ChartThemeDark) {
            pal.setColor(QPalette::Window, QRgb(0x121218));
            pal.setColor(QPalette::WindowText, QRgb(0xd6d6d6));
        } else if (theme == QtChartsCompat::Chart::ChartThemeBlueCerulean) {
            pal.setColor(QPalette::Window, QRgb(0x40434a));
            pal.setColor(QPalette::WindowText, QRgb(0xd6d6d6));
        } else if (theme == QtChartsCompat::Chart::ChartThemeBrownSand) {
            pal.setColor(QPalette::Window, QRgb(0x9e8965));
            pal.setColor(QPalette::WindowText, QRgb(0x404044));
        } else if (theme == QtChartsCompat::Chart::ChartThemeBlueNcs) {
            pal.setColor(QPalette::Window, QRgb(0x018bba));
            pal.setColor(QPalette::WindowText, QRgb(0x404044));
        } else if (theme == QtChartsCompat::Chart::ChartThemeHighContrast) {
            pal.setColor(QPalette::Window, QRgb(0xffab03));
            pal.setColor(QPalette::WindowText, QRgb(0x181818));
        } else if (theme == QtChartsCompat::Chart::ChartThemeBlueIcy) {
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
    for (QtChartsCompat::ChartView *chart : m_ChartMap)
        chart->setRenderHint(QPainter::Antialiasing, checked);

    // Update animation options
    QtChartsCompat::Chart::AnimationOptions options(
        ui->animation->itemData(ui->animation->currentIndex()).toInt());
    if (!m_ChartMap.isEmpty() && m_ChartMap.first()->chart()->animationOptions() != options) {
        for (QtChartsCompat::ChartView *chartView : m_ChartMap)
            chartView->chart()->setAnimationOptions(options);
    }

    // Update legend alignment
    Qt::Alignment alignment(
                ui->legend->itemData(ui->legend->currentIndex()).toInt());

    if (!alignment) {
        for (QtChartsCompat::ChartView *chartView : m_ChartMap)
            chartView->chart()->legend()->hide();
    } else {
        for (QtChartsCompat::ChartView *chartView : m_ChartMap) {
            chartView->chart()->legend()->setAlignment(alignment);
            chartView->chart()->legend()->show();
        }
    }

}

void QDevioDisplayModelUi::populateThemeBox()
{
    // add items to theme combobox
    ui->theme->addItem("Light", QtChartsCompat::Chart::ChartThemeLight);
    ui->theme->addItem("Blue Cerulean", QtChartsCompat::Chart::ChartThemeBlueCerulean);
    ui->theme->addItem("Dark", QtChartsCompat::Chart::ChartThemeDark);
    ui->theme->addItem("Brown Sand", QtChartsCompat::Chart::ChartThemeBrownSand);
    ui->theme->addItem("Blue NCS", QtChartsCompat::Chart::ChartThemeBlueNcs);
    ui->theme->addItem("High Contrast", QtChartsCompat::Chart::ChartThemeHighContrast);
    ui->theme->addItem("Blue Icy", QtChartsCompat::Chart::ChartThemeBlueIcy);
    ui->theme->addItem("Qt", QtChartsCompat::Chart::ChartThemeQt);
}

void QDevioDisplayModelUi::populateAnimationBox()
{
    // add items to animation combobox
    ui->animation->addItem("No Animations", QtChartsCompat::Chart::NoAnimation);
    ui->animation->addItem("GridAxis Animations", QtChartsCompat::Chart::GridAxisAnimations);
    ui->animation->addItem("Series Animations", QtChartsCompat::Chart::SeriesAnimations);
    ui->animation->addItem("All Animations", QtChartsCompat::Chart::AllAnimations);
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
#include "LogCategories.h"
void QDevioDisplayModelUi::updateGrid() {

    while(auto item = ui->gridLayout->itemAt(0)) {
        ui->gridLayout->removeItem(item);
        delete item;
    }
    qCDebug(lcNodeEditor) << "Col count: " << ui->gridLayout->columnCount();

    int i = 0;
    for (QtChartsCompat::ChartView *chartView : m_ChartMap) {
        ui->gridLayout->addWidget(chartView, i/m_ColCount, i%m_ColCount);
        i++;
    }
}

void QDevioDisplayModelUi::gridChanged(int val)
{
    m_ColCount = val;
    updateGrid();
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
            series = new QVector<QtChartsCompat::LineSeries*>;
            m_SeriesMap[hndl] = series;
        }
        QtChartsCompat::ValueAxis *axisX = new QtChartsCompat::ValueAxis;
        axisX->setRange(0, 8000);
        axisX->setLabelFormat("%g");
        axisX->setTitleText("Samples");
        QtChartsCompat::ValueAxis *axisY = new QtChartsCompat::ValueAxis;


        axisY->setRange(-3, 3);
        axisY->setTitleText("Audio level");
        for(int j = 0; j < num; j++) {
            auto seria = new QtChartsCompat::LineSeries;
            seria->setName(QString("Seria %1").arg(j));

            series->append(seria);
            for(int i=0;i<8000;i++){
                series->value(j)->append(i, -1 + 0.5*j);
            }
            chart->addSeries(series->value(j));
        }

        chart->createDefaultAxes();

        chart->setTitle("Data from the microphone");

        Qt::Alignment alignment(
                    ui->legend->itemData(ui->legend->currentIndex()).toInt());
        auto *axX = chart->axes(Qt::Horizontal).value(0);
        auto *axY = chart->axes(Qt::Vertical).value(0);
        if (axX) axX->setRange(0, 8000);
        if (axY) axY->setRange(-3, 3);
        chart->legend()->setAlignment(Qt::AlignLeft);

        if (!alignment) {
            for (QtChartsCompat::ChartView *chartView : m_ChartMap)
                chartView->chart()->legend()->hide();
        } else {
            for (QtChartsCompat::ChartView *chartView : m_ChartMap) {
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
    int removed = 0;
#if 0
    for (auto *chartView : m_ChartMap) {
        if (!chartView || !chartView->chart()) {
            continue;
        }

        auto *chart = chartView->chart();
        auto *series = m_SeriesMap.value(m_ChartMap.key(chartView), nullptr);

        if (!series) {
            continue;
        }

        while (!series->isEmpty()) {
            auto *first = series->first();
            chart->removeSeries(first);
            delete first;
            series->removeFirst();
            ++removed;
        }
    }
#endif
    return removed;
}

void QDevioDisplayModelUi::contextMenuEvent(QContextMenuEvent *event)
{

    QMenu menu;
    QAction *showAction    = menu.addAction("Show Chart Control");
    showAction->setCheckable(true);
    showAction->setChecked(ui->chartControl->isVisible());
    menu.addSeparator();
    QAction *fftAction = menu.addAction("Show FFT Spectrum");
    fftAction->setCheckable(true);
    fftAction->setChecked(m_ChartMap.value(m_fftHandle, nullptr) &&
                          m_ChartMap[m_fftHandle]->isVisible());
    QAction *selectedAction = menu.exec( event->globalPos() );

    if(showAction == selectedAction) {
        ui->chartControl->setVisible(selectedAction->isChecked());
    } else if (fftAction == selectedAction) {
        showFftDialog();
    }

}

void QDevioDisplayModelUi::pollData() {

}

void QDevioDisplayModelUi::bufferReady(QVector<QPointF> &buff, int channel)
{
    auto series = m_SeriesMap.value(0, nullptr);
    Q_ASSERT(series != nullptr);
    auto seria = series->value(channel);
    if(seria != nullptr)
        seria->replace(buff);

    if (channel == 0 && m_ChartMap.value(m_fftHandle, nullptr) &&
        m_ChartMap[m_fftHandle]->isVisible()) {
        QVector<QPointF> spectrum;
        computeFFT(buff, spectrum);
        auto *fftSeries = m_SeriesMap.value(m_fftHandle, nullptr);
        if (fftSeries && !fftSeries->isEmpty())
            fftSeries->first()->replace(spectrum);
    }
}

void QDevioDisplayModelUi::showFftDialog()
{
    auto *fftChartView = m_ChartMap.value(m_fftHandle, nullptr);
    if (!fftChartView)
        return;

    fftChartView->setVisible(!fftChartView->isVisible());
    updateGrid();
}

void QDevioDisplayModelUi::computeFFT(const QVector<QPointF> &timeDomainData,
                                      QVector<QPointF> &spectrumOut)
{
    const int n = timeDomainData.size();
    int fftSize = largestPowerOfTwoLe(n);
    if (fftSize < 2)
        return;

    if (fftSize != m_fftSize) {
        m_fftSize = fftSize;
        // Precompute Hann window
        m_fftWindow.resize(fftSize);
        for (int i = 0; i < fftSize; ++i)
            m_fftWindow[i] = 0.5 * (1.0 - std::cos(2.0 * M_PI * i / (fftSize - 1)));
    }

    QVector<double> re(fftSize);
    QVector<double> im(fftSize, 0.0);
    for (int i = 0; i < fftSize; ++i)
        re[i] = timeDomainData[i].y() * m_fftWindow[i];

    // Bit reversal
    for (int i = 1, j = 0; i < fftSize; ++i) {
        int bit = fftSize >> 1;
        for (; (j & bit) != 0; bit >>= 1)
            j ^= bit;
        j ^= bit;
        if (i < j) {
            std::swap(re[i], re[j]);
            std::swap(im[i], im[j]);
        }
    }

    // Cooley-Tukey radix-2 DIT
    for (int len = 2; len <= fftSize; len <<= 1) {
        const double ang = 2.0 * M_PI / len;
        const double wRe = std::cos(ang);
        const double wIm = -std::sin(ang);
        for (int i = 0; i < fftSize; i += len) {
            double curRe = 1.0, curIm = 0.0;
            const int half = len / 2;
            for (int j = 0; j < half; ++j) {
                const double tRe = curRe * re[i + j + half] - curIm * im[i + j + half];
                const double tIm = curRe * im[i + j + half] + curIm * re[i + j + half];
                re[i + j + half] = re[i + j] - tRe;
                im[i + j + half] = im[i + j] - tIm;
                re[i + j] += tRe;
                im[i + j] += tIm;
                const double tmp = curRe * wRe - curIm * wIm;
                curIm = curRe * wIm + curIm * wRe;
                curRe = tmp;
            }
        }
    }

    const int half = fftSize / 2;
    spectrumOut.resize(half);
    auto *fftChartView = m_ChartMap.value(m_fftHandle, nullptr);
    auto *axisX = fftChartView
        ? qobject_cast<QtChartsCompat::ValueAxis *>(fftChartView->chart()->axes(Qt::Horizontal).value(0)) : nullptr;

    double maxMag = 0.0;
    for (int i = 0; i < half; ++i) {
        const double mag = std::sqrt(re[i] * re[i] + im[i] * im[i]) / double(fftSize);
        if (mag > maxMag) maxMag = mag;
        spectrumOut[i] = QPointF(double(i), mag);
    }

    if (axisX)
        axisX->setRange(0, half);

    auto *axisY = fftChartView
        ? qobject_cast<QtChartsCompat::ValueAxis *>(fftChartView->chart()->axes(Qt::Vertical).value(0)) : nullptr;
    if (axisY)
        axisY->setRange(0, qMax(maxMag * 1.1, 0.001));
}
