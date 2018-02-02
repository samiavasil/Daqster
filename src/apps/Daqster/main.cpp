#include <QApplication>
#include "mainwindow.h"
#include "debug.h"
#include"QPluginManager.h"
#include<AppToolbar.h>
#include<QBasePluginObject.h>
#include<QCommandLineParser>

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

void PluginsInit()
{
/*TODO:  Move this on some initialization routine*/
Daqster::QPluginManager* PluginManager = Daqster::QPluginManager::instance();
PluginManager->SearchForPlugins();
if( NULL != PluginManager )
{
    qDebug() << "Plugin Manager: " << PluginManager;
  //  PluginManager->SearchForPlugins();
    //PluginManager->ShowPluginManagerGui();
    QList<Daqster::PluginDescription> PluginsList = PluginManager->GetPluginList();
    /*Just try to load/unload all plugins in initialization phase*/
    foreach ( const Daqster::PluginDescription& Desc, PluginsList) {
       for( int i=0;i < 1; i++){
            PluginManager->CreatePluginObject( Desc.GetProperty(PLUGIN_HASH).toString(), NULL )->deleteLater();
       }
    }
}
}

int main(int argc, char *argv[])
{
//    msg m;
#if QT_VERSION < QT_VERSION_CHECK(5, 0, 0)
    qInstallMsgHandler(m.myMessageOutput);
#else
 //   qInstallMessageHandler(m.myMessageOutput);
#endif
    //TODO: Check argument parser: http://doc.qt.io/qt-5/qcommandlineparser.html
    QApplication a(argc, argv);
    QApplication::setApplicationName("Daqster");
    QApplication::setApplicationVersion("0.1");

    QCommandLineParser parser;
    parser.setApplicationDescription("This program is used to run Daqster Application plugins");
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addPositionalArgument("apps ...", QCoreApplication::translate("main", "One ore more Application plugin names which will be automaticaly started"));


    // An option with a value
    QCommandLineOption targetDirectoryOption(QStringList() << "t" << "target-directory",
            QCoreApplication::translate("main", "Copy all source files into <directory>."),
            QCoreApplication::translate("main", "directory"));
    parser.addOption(targetDirectoryOption);

    // Process the actual command line arguments given by the user
    parser.process(a);


     const QStringList args = parser.positionalArguments();

     qDebug() << "Positional Argumments: " << args;

  // MainWindow w;
   // w.show();
qDebug() << __BASE_FILE__  << __FILE__;
    /*For correct plugoins shutdown behaviour QPluginManager initialization should be called. */
    if( !Daqster::QPluginManager::instance()->Initialize() ){
        DEBUG << "QPluginManager Initialization Error" ;
    }

    DEBUG << "Show window";
    PluginsInit();
    AppToolbar ApTooolbar;
    ApTooolbar.show();
    int res = a.exec();
    //Daqster::QPluginManager::instance()->ShutdownPluginManager();
    return res;
}
