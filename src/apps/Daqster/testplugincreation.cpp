#include "testplugincreation.h"
#include <base/QPluginManager.h>
#include<QDebug>
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


void TestPluginCreation::run()
{
    Daqster::QPluginManager* PluginManager = Daqster::QPluginManager::instance();
    if( NULL != PluginManager )
    {
        qDebug() << "Plugin Manager: " << PluginManager;
        PluginManager->SearchForPlugins();
        //PluginManager->ShowPluginManagerGui();
        QList<Daqster::PluginDescription> PluginsList = PluginManager->GetPluginList();
        foreach ( Daqster::PluginDescription Desc, PluginsList) {
           PluginManager->CreatePluginObject( Desc.GetProperty(PLUGIN_HASH).toString() );
        }
    }
}
