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
class QChartView;
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
    virtual int SetSeries(disp_hndl_t hndl, int num);
    virtual int RemoveSeries();

protected:
    void contextMenuEvent(QContextMenuEvent *event) override;
    void populateThemeBox();
    void populateAnimationBox();
    void populateLegendBox();
    void updateGrid();
private:
    Ui::QDevioDisplayModelUi *ui;
/*    QVector<QLineSeries*> m_series;*/
    QMap<disp_hndl_t, QVector<QLineSeries*>*> m_SeriesMap;
    QMap<disp_hndl_t, QChartView*> m_ChartMap;
//    QChart* m_chart;
    disp_hndl_t m_NextHndl;
    int m_ColCount;
public slots:
    void bufferReady(QVector<QPointF>& buff, int channel);
protected slots:
    void pollData();
    void updateUI();
    void gridChanged(int val);
};

#endif // QDEVIODISPLAYMODELUI_H
