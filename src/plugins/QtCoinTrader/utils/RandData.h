#ifndef DATA_H
#define DATA_H

#include <QObject>
#include <QPointF>
#include <QTimer>
#include <QList>


#include <QtCharts/QLineSeries>

using namespace QtCharts;
//   m_wSeries;

class RandData : public QLineSeries
{
    Q_OBJECT
    Q_PROPERTY(QPointF wValue READ wValue NOTIFY wValueChanged)



public:
    RandData(QObject *parent=Q_NULLPTR);
    QPointF wValue() const{
        return m_wValue;
    }

signals:
    void wValueChanged();
private slots:
    void wTimeout();
private:
    QTimer * m_wTimer;
    QPointF m_wValue;
    int m_Phase;
};

#endif // DATA_H
