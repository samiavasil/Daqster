#include "DaqsterTeplateInterface.h"
#include "base/debug.h"
#include<QLabel>
#include<QLayout>
#include "base/QPluginManager.h"

TemplatePluginObject::TemplatePluginObject(QObject *Parent):QBasePluginObject ( Parent  ){
    m_Win = new QMainWindow();
    QLabel* label = new QLabel( );
    label->setText("PluginTemplate Demo");
    m_Win->setCentralWidget(label);
    m_Win->show();
    m_Win->setAttribute(Qt::WA_DeleteOnClose, true);
    connect( m_Win, SIGNAL(destroyed(QObject*)), this, SLOT(MainWinDestroyed(QObject*)) );

    Daqster::QPluginManager& pm = Daqster::GetApplicationPluginManager();
    pm.SearchForPlugins();
    pm.ShowPluginManagerGui();

}

TemplatePluginObject::~TemplatePluginObject()
{
    DEBUG_V << "TemplatePluginObject destroyed";
}

void TemplatePluginObject::MainWinDestroyed( QObject* obj )
{
    m_Win = NULL;
    deleteLater();
    if( NULL == obj )
        DEBUG << "Strange::!!!";

}

DaqsterTeplateInterface::DaqsterTeplateInterface(QObject* parent ):QPluginObjectsInterface(parent)
{
    DEBUG << "DaqsterTeplateInterface object create";
    QIcon icon( QString::fromUtf8(":/template.png") );
    m_PluginDescryptor.SetIcon( icon );
    m_PluginDescryptor.SetName( "PluginTemplate" );
    m_PluginDescryptor.SetPluginType( SOME_TYPE );
    m_PluginDescryptor.SetPluginTypeName("SOME_TYPE");
    m_PluginDescryptor.SetVersion( "0.0.1" );
    m_PluginDescryptor.SetDescription( "MyPluginTemplate" );
    char docstr[] = \
    "This is a basic Daqster plugin template and can be used for implementing a new type daqster plugin \n\
    \n\
    Here you can add detailed description of the plugin...";
    m_PluginDescryptor.SetDetailDescription( QObject::tr( docstr ) );
    m_PluginDescryptor.SetLicense( QObject::tr("The plugin's license have to be.....") );
    m_PluginDescryptor.SetAuthor( "Plugin Author" );
}

DaqsterTeplateInterface::~DaqsterTeplateInterface(  )
{
    DEBUG << "DaqsterTeplateInterface object delete";
}

Daqster::QBasePluginObject *DaqsterTeplateInterface::CreatePluginInternal(QObject *Parrent)
{
    return new TemplatePluginObject(Parrent);
}

#if QT_VERSION < QT_VERSION_CHECK(5, 0, 0)
Q_EXPORT_PLUGIN2(DaqsterTemlatePlugin, DaqsterTeplateInterface)
#endif


