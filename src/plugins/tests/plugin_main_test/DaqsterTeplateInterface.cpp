#include "DaqsterTeplateInterface.h"
#include "debug.h"
#include "TemplatePluginObject.h"

DaqsterTeplateInterface::DaqsterTeplateInterface(QObject* parent ):QPluginObjectsInterface(parent)
{
    Q_INIT_RESOURCE(main_test);
    DEBUG << "DaqsterTeplateInterface object create";
    QIcon icon( QString::fromUtf8(":/main.png") );
    m_PluginDescryptor.SetIcon( icon );
    m_PluginDescryptor.SetProperty( PLUGIN_NAME, "PluginMainTest" );
    m_PluginDescryptor.SetProperty( PLUGIN_TYPE, Daqster::PluginDescription::SOME_TYPE );
    m_PluginDescryptor.SetProperty( PLUGIN_TYPE_NAME, "SOME_TYPE" );
    m_PluginDescryptor.SetProperty( PLUGIN_VERSION, "0.0.1" );
    m_PluginDescryptor.SetProperty( PLUGIN_DESCRIPTION, "Plugin Main Test" );
    char docstr[] = \
    "PluginMainTest is a basic Daqster plugin test. \n\
    \n\
    Here you can add detailed description of the plugin...";
    m_PluginDescryptor.SetProperty( PLUGIN_DETAIL_DESCRIPTION, QObject::tr( docstr ) );
    m_PluginDescryptor.SetProperty( PLUGIN_LICENSE, QObject::tr( "The plugin's license have to be....." ) );
    m_PluginDescryptor.SetProperty( PLUGIN_AUTHOR, "Vasil Vasilev" );
}

DaqsterTeplateInterface::~DaqsterTeplateInterface(  )
{
    DEBUG << "DaqsterTeplateInterface object delete";
}

Daqster::QBasePluginObject *DaqsterTeplateInterface::CreatePluginInternal(QObject *Parrent)
{
    TemplatePluginObject* Obj = new TemplatePluginObject(Parrent);
    if( NULL != Obj ){
        Obj->SetName( m_PluginDescryptor.GetProperty(PLUGIN_NAME).toString() );
    }
    return Obj;
}

#if QT_VERSION < QT_VERSION_CHECK(5, 0, 0)
Q_EXPORT_PLUGIN2(DaqsterTemlatePlugin, DaqsterTeplateInterface)
#endif


