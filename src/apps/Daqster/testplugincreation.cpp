#include "testplugincreation.h"
#include <QPluginManager.h>
#include<QDebug>
#include<QBasePluginObject.h>

#define GUI_PLUG_NUM (10)
TestPluginCreation::TestPluginCreation()
{
    qDebug() << "TestPluginCreation created ";
}

void TestPluginCreation::stopRunning()
{
    isRunning = 0;
    qDebug() << "TestPluginCreation finish ";
}


void TestPluginCreation::run(QObject *Parent)
{
    Daqster::QPluginManager* PluginManager = Daqster::QPluginManager::instance();
    if( NULL != PluginManager )
    {
        qDebug() << "Plugin Manager: " << PluginManager;
      //  PluginManager->SearchForPlugins();
        //PluginManager->ShowPluginManagerGui();
        QList<Daqster::PluginDescription> PluginsList = PluginManager->GetPluginList();
        Daqster::QBasePluginObject* obj;
        foreach ( const Daqster::PluginDescription& Desc, PluginsList) {
           for( int i=0;i < 1; i++){
                obj = PluginManager->CreatePluginObject( Desc.GetProperty(PLUGIN_HASH).toString(), Parent );
                if( NULL != obj ){
                    obj->Initialize();
                }
           }
        }
    }
}
