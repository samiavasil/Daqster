#include "RestApiTesterInterface.h"
#include "debug.h"
#include "RestApiTesterPluginObject.h"

RestApiTesterInterface::RestApiTesterInterface(QObject* parent ):QPluginInterface(parent)
{
    Q_INIT_RESOURCE(RestApiTester);
    DEBUG << "DaqsterTeplateInterface object create";
    QIcon icon( QString::fromUtf8(":/RestApiTester.png") );
    m_PluginDescryptor.SetIcon( icon );
    m_PluginDescryptor.SetProperty( PLUGIN_NAME, "RestApiTester" );
    m_PluginDescryptor.SetProperty( PLUGIN_TYPE, Daqster::PluginDescription::APPLICATION_PLUGIN );
    m_PluginDescryptor.SetProperty( PLUGIN_TYPE_NAME, "This is a plugin application for coin trading" );
    m_PluginDescryptor.SetProperty( PLUGIN_VERSION, "0.0.1" );
    m_PluginDescryptor.SetProperty( PLUGIN_DESCRIPTION, "RestApiTester Plugin" );
    char docstr[] = \
    "This is a basic Daqster plugin template and can be used for implementing a new type daqster plugin \n\
    \n\
    Here you can add detailed description of the plugin...";
    m_PluginDescryptor.SetProperty( PLUGIN_DETAIL_DESCRIPTION, QObject::tr( docstr ) );
    m_PluginDescryptor.SetProperty( PLUGIN_LICENSE, QObject::tr( "The plugin's license have to be....." ) );
    m_PluginDescryptor.SetProperty( PLUGIN_AUTHOR, "Vasil Vasilev" );
}

RestApiTesterInterface::~RestApiTesterInterface(  )
{
    DEBUG << "DaqsterTemplateInterface object delete";
}

Daqster::QBasePluginObject *RestApiTesterInterface::CreatePluginInternal(QObject *Parrent)
{
    RestApiTesterPluginObject* Obj = new RestApiTesterPluginObject(Parrent);
    if( NULL != Obj ){
        Obj->SetName( m_PluginDescryptor.GetProperty(PLUGIN_NAME).toString() );
    }
    return Obj;
}

#if QT_VERSION < QT_VERSION_CHECK(5, 0, 0)
Q_EXPORT_PLUGIN2(RestApiTesterPlugin, RestApiTesterInterface)
#endif


