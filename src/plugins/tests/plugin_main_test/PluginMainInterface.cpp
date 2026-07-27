#include "PluginMainInterface.h"
#include "debug.h"
#include "PluginMainObject.h"

PluginMainInterface::PluginMainInterface(QObject* parent ):QPluginInterface(parent)
{
    Q_INIT_RESOURCE(main_test);
    DEBUG << "PluginMainInterface object create";
    QIcon icon( QString::fromUtf8(":/main.png") );
    m_PluginDescriptor.SetIcon( icon );
    m_PluginDescriptor.SetProperty( PLUGIN_NAME, "PluginMainTest" );
    m_PluginDescriptor.SetProperty( PLUGIN_TYPE, Daqster::PluginDescription::APPLICATION_PLUGIN );
    m_PluginDescriptor.SetProperty( PLUGIN_TYPE_NAME, "SOME_TYPE" );
    m_PluginDescriptor.SetProperty( PLUGIN_VERSION, "0.0.1" );
    m_PluginDescriptor.SetProperty( PLUGIN_DESCRIPTION, "Plugin Main Test" );
    char docstr[] = \
    "PluginMainTest is a basic Daqster plugin test. \n\
    \n\
    Here you can add detailed description of the plugin...";
    m_PluginDescriptor.SetProperty( PLUGIN_DETAIL_DESCRIPTION, QObject::tr( docstr ) );
    m_PluginDescriptor.SetProperty( PLUGIN_LICENSE, QObject::tr( "The plugin's license have to be....." ) );
    m_PluginDescriptor.SetProperty( PLUGIN_AUTHOR, "Vasil Vasilev" );
}

PluginMainInterface::~PluginMainInterface(  )
{
    DEBUG << "PluginMainInterface object delete";
}

Daqster::QBasePluginObject *PluginMainInterface::CreatePluginInternal(QObject *Parrent)
{
    PluginMainObject* Obj = new PluginMainObject(Parrent);
    if( nullptr != Obj ){
        Obj->SetName( m_PluginDescriptor.GetProperty(PLUGIN_NAME).toString() );
    }
    return Obj;
}

#if QT_VERSION < QT_VERSION_CHECK(5, 0, 0)
Q_EXPORT_PLUGIN2(PluginMainTest, PluginMainInterface)
#endif


