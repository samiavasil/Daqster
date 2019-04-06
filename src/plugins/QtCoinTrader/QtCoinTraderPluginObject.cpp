#include "QtCoinTraderPluginObject.h"
#include "QPluginManager.h"
#include "debug.h"
#include<QMainWindow>

QtCoinTraderPluginObject::QtCoinTraderPluginObject(QObject *Parent):QBasePluginObject ( Parent  ),m_Win(NULL){

}

QtCoinTraderPluginObject::~QtCoinTraderPluginObject()
{
    DeInitialize();
}


void QtCoinTraderPluginObject::SetName(const QString &name)
{
    if( m_Win )
    {
        m_Win->setWindowTitle( name );
    }
}

#include <QQmlApplicationEngine>

#include <QQmlApplicationEngine>
#include <QQuickStyle>
#include <QGuiApplication>

#if 0
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include "api/models/couponmodel.h"
#include "QtAwesomeAndroid.h"
#include <QTranslator>
#include <QSettings>

#include "jsonrestlistmodel.h"

int main(int argc, char *argv[])
{
    QGuiApplication::setApplicationName("Skid.KZ");
    QGuiApplication::setApplicationVersion("1.0");
    QGuiApplication::setOrganizationName("Forsk.Ru");
    QGuiApplication::setAttribute(Qt::AA_EnableHighDpiScaling);

    QGuiApplication app(argc, argv);

    //i18n
    QString languageCode = QLocale::system().name();
    if (languageCode.contains("_")) {
        languageCode = languageCode.split("_").at(0);
    }
    QString fileName = "skidkz_" + languageCode;

    QTranslator qtTranslator;
    if ( !qtTranslator.load(fileName, ":/i18n/") ){
        qDebug() << "Translation file not loaded:" << fileName;
        qDebug() << "Language " << languageCode << " not supported yet";
    }
    app.installTranslator(&qtTranslator);

    //Font Awesome
    QtAwesomeAndroid* awesome = new QtAwesomeAndroid( qApp );
    awesome->setDefaultOption( "color", QColor(255,255,255) );
    awesome->initFontAwesome();

    //api and models
    SkidKZApi::declareQML();
    CouponModel::declareQML();
    JsonRestListModel::declareQML();

    //settings
    QSettings settings;
    qputenv("QT_LABS_CONTROLS_STYLE", settings.value("style").toByteArray());

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty("awesome", awesome);
    engine.load(QUrl(QStringLiteral("qrc:/main.qml")));

    return app.exec();
}
#endif

#include"qtrest_lib.h"
#include"utils/RandData.h"
bool QtCoinTraderPluginObject::Initialize()
{

    QGuiApplication::setApplicationName("QtCoinTrader");
    QGuiApplication::setApplicationVersion("1.0");
    QGuiApplication::setOrganizationName("Samiavasil");
    QGuiApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    initializeRest();
    qmlRegisterType<RandData>("com.github.samiavasil.cointrader.randdata", 1, 0, "RandData");
    //    m_Win = new QMainWindow();
    //    m_Win->show();
    QQuickStyle::setStyle("Material");
    QStringList lis = QQuickStyle::availableStyles();
    QQmlApplicationEngine* engine = new QQmlApplicationEngine(m_Win);
    //engine.rootContext()->setContextProperty("awesome", awesome);
    //engine->rootContext()->setContextProperty("dataFromCpp", new RandData());
    engine->load(QUrl(QStringLiteral("qrc:/qml/About.qml")));
    //   engine->load(QUrl(QStringLiteral("qrc:/main.qml")));


    /* m_Win = new QMainWindow();
    QLabel* label = new QLabel( );
    label->setText("QTCoinTrader Plugin");
    m_Win->setCentralWidget(label);
    QPushButton* button = new QPushButton(m_Win);

    m_Win->show();
    m_Win->setAttribute(Qt::WA_DeleteOnClose, true);
    connect( m_Win, SIGNAL(destroyed(QObject*)), this, SLOT(MainWinDestroyed(QObject*)) );
    connect( button, SIGNAL(clicked(bool)), this, SLOT(ShowPlugins()) );*/
}

void QtCoinTraderPluginObject::DeInitialize()
{
    if( m_Win ){
        m_Win->deleteLater();
    }
    DEBUG_V << "TemplatePluginObject destroyed";
}

void QtCoinTraderPluginObject::MainWinDestroyed( QObject* obj )
{
    m_Win = NULL;
    deleteLater();
    if( NULL == obj )
        DEBUG << "Strange::!!!";

}

void QtCoinTraderPluginObject::ShowPlugins()
{
    Daqster::QPluginManager* pm = Daqster::QPluginManager::instance();
    if( NULL != pm )
    {
        DEBUG << "Plugin Manager: " << pm;
        //     pm->SearchForPlugins();
        pm->ShowPluginManagerGui( m_Win );
    }
}
