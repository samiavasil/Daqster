#ifndef DEBUG_H
#define DEBUG_H

//#define FULL_DUMP
#define FULL_VERBOSE_DUMP
#define ENABLE_DUMP
#include  <QDebug>
#include  <QtGlobal>

#if defined( ENABLE_VERBOSE_DUMP ) || defined( FULL_VERBOSE_DUMP )
    /*#define DEBUG QDebug(QtDebugMsg)<<QString("DBG file:///%1:%2:0").arg(__FILE__).arg(__LINE__)<<": "*/
    #define DEBUG            QDebug(QtDebugMsg)<<"DBG:   "<<__FILE__<<" Line:"<<__LINE__<<": "
    #define DEBUG_V          QDebug(QtDebugMsg)<<"DBG_V: "<<__FILE__<<" Line:"<<__LINE__<<": "
#elif defined( ENABLE_DUMP ) || defined( FULL_DUMP )
    #define DEBUG            QDebug(QtDebugMsg)<<"DBG:   "<<__FILE__<<" Line:"<<__LINE__<<": "
    #define DEBUG_V          while(false) QNoDebug()
#else
    #define DEBUG            while(false) QNoDebug()
    #define DEBUG_V          while(false) QNoDebug()
#endif

#define WARNING              QDebug(QtWarningMsg) <<"Warn:  "<<__FILE__<<" Line:"<<__LINE__<<": "
#define CRITICAL             QDebug(QtCriticalMsg)<<"Critic:"<<__FILE__<<" Line:"<<__LINE__<<": "
#define FATAL                QDebug(QtFatalMsg)   <<"Fatal: "<<__FILE__<<" Line:"<<__LINE__<<": "

#endif // DEBUG_H
