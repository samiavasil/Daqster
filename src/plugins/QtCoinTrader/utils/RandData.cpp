#include "RandData.h"
#include<QDebug>
#include<math.h>

void RandData::wTimeout(){
    int HIGH = 100;
    int LOW = 0;
    int val = rand() % (HIGH - LOW + 1) + LOW;
    m_wValue.setX( count()-1 );
    m_wValue.setY(val);
//    append(m_wValue);
    //emit wValueChanged();
    setName("RandData");
//    qDebug() << count() << "  " << m_wValue ;

#if 0
    for( int i=0; i < count()-1 ; i++  ) {
        replace( i, i , at(i+1).y() );
    }
    replace(count()-1,m_wValue);
#else
    QList<QPointF>  pt = points();
    //QVector<QPointF> pt = pointsVector();
    int i;
    qreal v = pt[0].y();

int cnt = pt.count();
    for( i=0; i < cnt ; i++  ) {
      //  replace( i, i , at(i+1).y() );
        pt[i].setX( (m_Phase + i)%cnt );
        //pt[i] = {i,pt[i+1].y()};
    //    pt[i] = {i,100*sin( (2*3.14* i/pt.count()) + (2*3.14*i/pt.count()) ) };
    }
  //  pt[pt.count()-1].setY(v);
   // pt[pt.count()-1]=m_wValue;
    m_Phase+=1;
  replace(pt);
#endif
}

RandData::RandData(QObject *parent):QLineSeries(parent),m_wValue(0,0),m_Phase(0) {
    m_wTimer = new QTimer(this);
    m_wTimer->setInterval(5);
    setPointsVisible(true);
    setColor(QColor("red"));
    setUseOpenGL(true);
    int Pnum = 256;
    for(int i=0;i<Pnum;i++){
        int HIGH = 100;
        int LOW = 0;
        int val = rand() % (HIGH - LOW + 1) + LOW;
        m_wValue.setX(i);
        m_wValue.setY(100*sin( ((2*3.14* i)/Pnum) ));
        append(m_wValue);
    }
    qDebug() << "Points: " << points();
    connect(m_wTimer, &QTimer::timeout, this, &RandData::wTimeout);
    m_wTimer->start();

}
