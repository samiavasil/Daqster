#include "DemoNodeEditorNodesInterface.h"
#include "debug.h"
#include "DemoNodeEditorNodesObject.h"

DemoNodeEditorNodesInterface::DemoNodeEditorNodesInterface(QObject* parent)
    : Daqster::QPluginInterface(parent)
{
    DEBUG << "DemoNodeEditorNodesInterface object create";
    m_PluginDescriptor.SetProperty(PLUGIN_NAME, "DemoNodeEditorNodes");
    m_PluginDescriptor.SetProperty(PLUGIN_TYPE, Daqster::PluginDescription::USER_DEFINED_TYPE);
    m_PluginDescriptor.SetProperty(PLUGIN_TYPE_NAME, "NODE_PROVIDER_PLUGIN");
    m_PluginDescriptor.SetProperty(PLUGIN_VERSION, "0.2.0");
    m_PluginDescriptor.SetProperty(PLUGIN_DESCRIPTION, "Demo node editor nodes: NumberSource, NumberDisplay, Modulo, Audio, LLM.");
    m_PluginDescriptor.SetProperty(PLUGIN_AUTHOR, "Vasil Vasilev");
}

DemoNodeEditorNodesInterface::~DemoNodeEditorNodesInterface()
{
    DEBUG << "DemoNodeEditorNodesInterface object delete";
}

Daqster::QBasePluginObject* DemoNodeEditorNodesInterface::CreatePluginInternal(QObject* Parrent)
{
    DemoNodeEditorNodesObject* Obj = new DemoNodeEditorNodesObject(Parrent);
    if (nullptr != Obj) {
        Obj->SetName(m_PluginDescriptor.GetProperty(PLUGIN_NAME).toString());
    }
    return Obj;
}
