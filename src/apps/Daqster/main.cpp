#include <QApplication>
#include "debug.h"
#include"QPluginManager.h"
#include<AppToolbar.h>
#include<QBasePluginObject.h>
#include<QCommandLineParser>
#include"ApplicationsManager.h"

#include"main.h"

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

//#include "RestApi.h"
#include<QLibrary>

int main(int argc, char *argv[])
{
    int res = 0;
    //    msg m;
#if QT_VERSION < QT_VERSION_CHECK(5, 0, 0)
    qInstallMsgHandler(m.myMessageOutput);
#else
    //   qInstallMessageHandler(m.myMessageOutput);
#endif
    //TODO: Check argument parser: http://doc.qt.io/qt-5/qcommandlineparser.html
QApplication::setAttribute(Qt::AA_ShareOpenGLContexts,true );
    QApplication a(argc, argv);
    QApplication::setApplicationName("Daqster");
    QApplication::setApplicationVersion("0.1");

    QCommandLineParser parser;
    parser.setApplicationDescription("This program is used to run Daqster Application plugins");
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addPositionalArgument("apps ...", QCoreApplication::translate("main", "One ore more Application plugin names which will be automaticaly started"));

//QLibrary lib("RestApi"); // QLibrary will try the platform's library suffix
//if(!lib.isLoaded())
//if (!lib.load()) {
//  qDebug() << "Library load error:" << lib.errorString();
//   exit(-1);
//}
    //RestApi::Api();//.SendRequest( RestApi::GET, "ui->urlEdit->text()" );
    // An option with a value
    QCommandLineOption targetDirectoryOption(QStringList() << "t" << "target-directory",
                                             QCoreApplication::translate("main", "Copy all source files into <directory>."),
                                             QCoreApplication::translate("main", "directory"));
    parser.addOption(targetDirectoryOption);

    // Process the actual command line arguments given by the user
    parser.process(a);


    const QStringList args = parser.positionalArguments();

    qDebug() << "Positional Argumments: " << args;

    Daqster::QPluginManager* PluginManager = Daqster::QPluginManager::instance();
    /*For correct plugoins shutdown behaviour QPluginManager initialization should be called. */
    if( !PluginManager->Initialize() ){
        DEBUG << "QPluginManager Initialization Error" ;
    }

    DEBUG << "Show window";
    PluginsInit();
qDebug() << "ARGS: " << args;
    if( args.count() > 0 ){
        Daqster::PluginFilter Filter;
        Filter.AddFilter( PLUGIN_TYPE, QString("%1").arg(Daqster::PluginDescription::APPLICATION_PLUGIN) );
        QList<Daqster::PluginDescription> PluginsList = PluginManager->GetPluginList( Filter );

        if( args.count() > 1 ){
            foreach( auto Name, args ) {
                ApplicationsManager::Instance().StartApplication( "./Daqster",QStringList(Name) );
            }
        }
        else{
            QString Name = args[0];
            Daqster::QBasePluginObject* obj = NULL;
            foreach ( const Daqster::PluginDescription& Desc, PluginsList ) {
                qDebug()<< "Desc: "<< Desc.GetProperty(PLUGIN_NAME).toString() << "\nName: "<<Name;
                if( 0 == Desc.GetProperty(PLUGIN_NAME).toString().compare(Name) ){
                    obj = PluginManager->CreatePluginObject( Desc.GetProperty(PLUGIN_HASH).toString(), NULL );
                    if( NULL != obj ){
                        obj->Initialize();
                        QApplication::setApplicationName(Desc.GetProperty(PLUGIN_HASH).toString());
                    }
                }
            }
        }
        res = a.exec();
    }
    else{

        // MainWindow w;
        // w.show();
        qDebug() << __BASE_FILE__  << __FILE__;
        AppToolbar AppBar;
        AppBar.show();
        res = a.exec();
    }

    //Daqster::QPluginManager::instance()->ShutdownPluginManager();
    return res;
}
