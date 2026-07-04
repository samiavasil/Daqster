#ifndef QDEVIODISPLAYMODELUI_H
#define QDEVIODISPLAYMODELUI_H

#include "QtChartsCompat.h"

#include <QWidget>

#include <QMap>
#include <QVector>

namespace Ui {
class QDevioDisplayModelUi;
}

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
/*    QVector<QtChartsCompat::LineSeries*> m_series;*/
    QMap<disp_hndl_t, QVector<QtChartsCompat::LineSeries*>*> m_SeriesMap;
    QMap<disp_hndl_t, QtChartsCompat::ChartView*> m_ChartMap;
//    QChart* m_chart;
    disp_hndl_t m_NextHndl;
    int m_ColCount;
    int m_fftHandle;
    QVector<double> m_fftWindow;
    void computeFFT(const QVector<QPointF> &timeDomainData, QVector<QPointF> &spectrumOut);
    int m_fftSize = 0;
public slots:
    void bufferReady(QVector<QPointF>& buff, int channel);
protected slots:
    void pollData();
    void updateUI();
    void gridChanged(int val);
private slots:
    void showFftDialog();
};

#endif // QDEVIODISPLAYMODELUI_H
