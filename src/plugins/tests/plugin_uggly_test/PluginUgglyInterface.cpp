#include "PluginUgglyInterface.h"
#include "debug.h"
#include "PluginUgglyObject.h"

PluginUgglyInterface::PluginUgglyInterface(QObject* parent ):QPluginInterface(parent)
{
    Q_INIT_RESOURCE(uggly_test);
    DEBUG << "DaqsterTeplateInterface object create";
    QIcon icon( QString::fromUtf8(":/uggly.png") );
    m_PluginDescriptor.SetIcon( icon );
    m_PluginDescriptor.SetProperty( PLUGIN_NAME, "UgglyTestPlugin" );
    m_PluginDescriptor.SetProperty( PLUGIN_TYPE, Daqster::PluginDescription::DETECT_BY_TYPE_NAME );
    m_PluginDescriptor.SetProperty( PLUGIN_TYPE_NAME, "SOME_TYPE" );
    m_PluginDescriptor.SetProperty( PLUGIN_VERSION, DAQSTER_PLUGIN_VERSION );
    m_PluginDescriptor.SetProperty( PLUGIN_DESCRIPTION, "Uggly Test Plugin" );
    char docstr[] = \
    "UgglyTestPlugin is a basic Daqster plugin template and can be used for implementing a new type daqster plugin \n\
    \n\
    Here you can add detailed description of the plugin...";
    m_PluginDescriptor.SetProperty( PLUGIN_DETAIL_DESCRIPTION, QObject::tr( docstr ) );
    m_PluginDescriptor.SetProperty( PLUGIN_LICENSE, QObject::tr( "The plugin's license have to be....." ) );
    m_PluginDescriptor.SetProperty( PLUGIN_AUTHOR, "Vasil Vasilev" );
}

PluginUgglyInterface::~PluginUgglyInterface(  )
{
    DEBUG << "PluginUgglyInterface object delete";
}

Daqster::QBasePluginObject *PluginUgglyInterface::CreatePluginInternal(QObject *Parrent)
{
    PluginUgglyObject* Obj = new PluginUgglyObject(Parrent);
    if( nullptr != Obj ){
        Obj->SetName( m_PluginDescriptor.GetProperty(PLUGIN_NAME).toString() );
    }
    return Obj;
}

#if QT_VERSION < QT_VERSION_CHECK(5, 0, 0)
Q_EXPORT_PLUGIN2(UgglyTestPlugin, PluginUgglyInterface)
#endif


