#include <QApplication>
#include "mainwindow.h"
#include "base/debug.h"


class msg{
public:

#if QT_VERSION < QT_VERSION_CHECK(5, 0, 0)
    static void myMessageOutput(QtMsgType type, const char *b)
     {
        QString msg;
        msg.sprintf("%s", b);
#else
     static void myMessageOutput(QtMsgType type, const QMessageLogContext &, const QString & msg)
      {
#endif

        switch (type) {
        case QtDebugMsg:
            fprintf(stderr, "%s\n", msg.toStdString().c_str());
            break;
        case QtWarningMsg:
         //TODO:   fprintf(stderr, "%s\n", msg);
            break;
        case QtCriticalMsg:
            fprintf(stderr, "%s\n", msg.toStdString().c_str());
            break;
        case QtFatalMsg:
            fprintf(stderr, "%s\n", msg.toStdString().c_str());
            abort();
        }
    }
};

int main(int argc, char *argv[])
{
//    msg m;
#if QT_VERSION < QT_VERSION_CHECK(5, 0, 0)
    qInstallMsgHandler(m.myMessageOutput);
#else
 //   qInstallMessageHandler(m.myMessageOutput);
#endif
    QApplication a(argc, argv);
    MainWindow w;
    w.show();
    DEBUG << "Show window";
    return a.exec();
}
