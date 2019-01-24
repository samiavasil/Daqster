#include "UnitTestInterface.h"
#include "debug.h"
#include "UnitTestPluginObject.h"

UnitTestInterface::UnitTestInterface(QObject* parent ):QPluginInterface(parent)
{
    Q_INIT_RESOURCE(unit_test);
    DEBUG << "DaqsterTeplateInterface object create";
    QIcon icon( QString::fromUtf8(":/unit_test.png") );
    m_PluginDescryptor.SetIcon( icon );
    m_PluginDescryptor.SetProperty( PLUGIN_NAME, "UnitTestTemplate" );
    m_PluginDescryptor.SetProperty( PLUGIN_TYPE, Daqster::PluginDescription::APPLICATION_PLUGIN );
    m_PluginDescryptor.SetProperty( PLUGIN_TYPE_NAME, "SOME_TYPE" );
    m_PluginDescryptor.SetProperty( PLUGIN_VERSION, "0.0.1" );
    m_PluginDescryptor.SetProperty( PLUGIN_DESCRIPTION, "UnitTest Plugin" );
    char docstr[] = \
    "This is a basic Daqster plugin template and can be used for implementing a new type daqster plugin \n\
    \n\
    Here you can add detailed description of the plugin...";
    m_PluginDescryptor.SetProperty( PLUGIN_DETAIL_DESCRIPTION, QObject::tr( docstr ) );
    m_PluginDescryptor.SetProperty( PLUGIN_LICENSE, QObject::tr( "The plugin's license have to be....." ) );
    m_PluginDescryptor.SetProperty( PLUGIN_AUTHOR, "Vasil Vasilev" );
}

UnitTestInterface::~UnitTestInterface(  )
{
    DEBUG << "UnitTestInterface object delete";
}

Daqster::QBasePluginObject *UnitTestInterface::CreatePluginInternal(QObject *Parrent)
{
    UnitTestPluginObject* Obj = new UnitTestPluginObject(Parrent);
    if( NULL != Obj ){
        Obj->SetName( m_PluginDescryptor.GetProperty(PLUGIN_NAME).toString() );
    }
    return Obj;
}

#if QT_VERSION < QT_VERSION_CHECK(5, 0, 0)
Q_EXPORT_PLUGIN2(UnitTestPlugin, UnitTestInterface)
#endif


