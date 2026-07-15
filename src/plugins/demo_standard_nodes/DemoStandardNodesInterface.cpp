#include "DemoStandardNodesInterface.h"
#include "debug.h"
#include "DemoStandardNodesObject.h"

DemoStandardNodesInterface::DemoStandardNodesInterface(QObject* parent)
    : Daqster::QPluginInterface(parent)
{
    DEBUG << "DemoStandardNodesInterface object create";
    m_PluginDescryptor.SetProperty(PLUGIN_NAME, "DemoStandardNodes");
    m_PluginDescryptor.SetProperty(PLUGIN_TYPE, Daqster::PluginDescription::APPLICATION_PLUGIN);
    m_PluginDescryptor.SetProperty(PLUGIN_TYPE_NAME, "SOME_TYPE");
    m_PluginDescryptor.SetProperty(PLUGIN_VERSION, "0.1.0");
    m_PluginDescryptor.SetProperty(PLUGIN_DESCRIPTION, "Demo standard nodes: NumberSource, NumberDisplay, and Modulo.");
    m_PluginDescryptor.SetProperty(PLUGIN_AUTHOR, "Vasil Vasilev");
}

DemoStandardNodesInterface::~DemoStandardNodesInterface()
{
    DEBUG << "DemoStandardNodesInterface object delete";
}

Daqster::QBasePluginObject* DemoStandardNodesInterface::CreatePluginInternal(QObject* Parrent)
{
    DemoStandardNodesObject* Obj = new DemoStandardNodesObject(Parrent);
    if (nullptr != Obj) {
        Obj->SetName(m_PluginDescryptor.GetProperty(PLUGIN_NAME).toString());
    }
    return Obj;
}
