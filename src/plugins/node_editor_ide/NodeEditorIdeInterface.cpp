#include "NodeEditorIdeInterface.h"
#include "debug.h"
#include "NodeEditorIdeObject.h"

NodeEditorIdeInterface::NodeEditorIdeInterface(QObject* parent)
    : Daqster::QPluginInterface(parent)
{
    Q_INIT_RESOURCE(node_editor);
    DEBUG << "NodeEditorIdeInterface object create";
    QIcon icon(QString::fromUtf8(":/node_editor.png"));
    m_PluginDescriptor.SetIcon(icon);
    m_PluginDescriptor.SetProperty(PLUGIN_NAME, "NodeEditorIDE");
    m_PluginDescriptor.SetProperty(PLUGIN_TYPE, Daqster::PluginDescription::APPLICATION_PLUGIN);
    m_PluginDescriptor.SetProperty(PLUGIN_TYPE_NAME, "SOME_TYPE");
    m_PluginDescriptor.SetProperty(PLUGIN_VERSION, DAQSTER_PLUGIN_VERSION);
    m_PluginDescriptor.SetProperty(PLUGIN_DESCRIPTION, "Node editor IDE with built-in nodes and plugin-based node discovery.");
    char docstr[] =
        "Node Editor IDE plugin.\n\n"
        "Provides a visual node editor with built-in Audio, LLaMA, and example nodes.\n"
        "Discovers and integrates external INodeProvider plugins at runtime.";
    m_PluginDescriptor.SetProperty(PLUGIN_DETAIL_DESCRIPTION, QObject::tr(docstr));
    m_PluginDescriptor.SetProperty(PLUGIN_LICENSE, QObject::tr("The plugin's license have to be....."));
    m_PluginDescriptor.SetProperty(PLUGIN_AUTHOR, "Vasil Vasilev");
}

NodeEditorIdeInterface::~NodeEditorIdeInterface()
{
    DEBUG << "NodeEditorIdeInterface object delete";
}

Daqster::QBasePluginObject* NodeEditorIdeInterface::CreatePluginInternal(QObject* Parrent)
{
    NodeEditorIdeObject* Obj = new NodeEditorIdeObject(Parrent);
    if (nullptr != Obj) {
        Obj->SetName(m_PluginDescriptor.GetProperty(PLUGIN_NAME).toString());
    }
    return Obj;
}
