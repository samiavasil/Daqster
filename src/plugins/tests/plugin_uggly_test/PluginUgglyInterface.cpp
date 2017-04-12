#include "PluginUgglyInterface.h"
#include "debug.h"
#include "PluginUgglyObject.h"

PluginFancyInterface::PluginFancyInterface(QObject* parent ):QPluginObjectsInterface(parent)
{
    Q_INIT_RESOURCE(uggly_test);
    DEBUG << "DaqsterTeplateInterface object create";
    QIcon icon( QString::fromUtf8(":/uggly.png") );
    m_PluginDescryptor.SetIcon( icon );
    m_PluginDescryptor.SetProperty( PLUGIN_NAME, "UgglyTestPlugin" );
    m_PluginDescryptor.SetProperty( PLUGIN_TYPE, Daqster::PluginDescription::DETECT_BY_TYPE_NAME );
    m_PluginDescryptor.SetProperty( PLUGIN_TYPE_NAME, "SOME_TYPE" );
    m_PluginDescryptor.SetProperty( PLUGIN_VERSION, "0.0.1" );
    m_PluginDescryptor.SetProperty( PLUGIN_DESCRIPTION, "Uggly Test Plugin" );
    char docstr[] = \
    "UgglyTestPlugin is a basic Daqster plugin template and can be used for implementing a new type daqster plugin \n\
    \n\
    Here you can add detailed description of the plugin...";
    m_PluginDescryptor.SetProperty( PLUGIN_DETAIL_DESCRIPTION, QObject::tr( docstr ) );
    m_PluginDescryptor.SetProperty( PLUGIN_LICENSE, QObject::tr( "The plugin's license have to be....." ) );
    m_PluginDescryptor.SetProperty( PLUGIN_AUTHOR, "Vasil Vasilev" );
}

PluginFancyInterface::~PluginFancyInterface(  )
{
    DEBUG << "DaqsterTeplateInterface object delete";
}

Daqster::QBasePluginObject *PluginFancyInterface::CreatePluginInternal(QObject *Parrent)
{
    PluginFancyObject* Obj = new PluginFancyObject(Parrent);
    if( NULL != Obj ){
        Obj->SetName( m_PluginDescryptor.GetProperty(PLUGIN_NAME).toString() );
    }
    return Obj;
}

#if QT_VERSION < QT_VERSION_CHECK(5, 0, 0)
Q_EXPORT_PLUGIN2(DaqsterTemlatePlugin, DaqsterTeplateInterface)
#endif


