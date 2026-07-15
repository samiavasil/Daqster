#include "NodeEditorIdeInterface.h"
#include "debug.h"
#include "NodeEditorIdeObject.h"

NodeEditorIdeInterface::NodeEditorIdeInterface(QObject* parent)
    : Daqster::QPluginInterface(parent)
{
    Q_INIT_RESOURCE(node_editor);
    DEBUG << "NodeEditorIdeInterface object create";
    QIcon icon(QString::fromUtf8(":/node_editor.png"));
    m_PluginDescryptor.SetIcon(icon);
    m_PluginDescryptor.SetProperty(PLUGIN_NAME, "NodeEditorIDE");
    m_PluginDescryptor.SetProperty(PLUGIN_TYPE, Daqster::PluginDescription::APPLICATION_PLUGIN);
    m_PluginDescryptor.SetProperty(PLUGIN_TYPE_NAME, "SOME_TYPE");
    m_PluginDescryptor.SetProperty(PLUGIN_VERSION, "0.2.0");
    m_PluginDescryptor.SetProperty(PLUGIN_DESCRIPTION, "Node editor IDE with built-in nodes and plugin-based node discovery.");
    char docstr[] =
        "Node Editor IDE plugin.\n\n"
        "Provides a visual node editor with built-in Audio, LLaMA, and example nodes.\n"
        "Discovers and integrates external INodeProvider plugins at runtime.";
    m_PluginDescryptor.SetProperty(PLUGIN_DETAIL_DESCRIPTION, QObject::tr(docstr));
    m_PluginDescryptor.SetProperty(PLUGIN_LICENSE, QObject::tr("The plugin's license have to be....."));
    m_PluginDescryptor.SetProperty(PLUGIN_AUTHOR, "Vasil Vasilev");
}

NodeEditorIdeInterface::~NodeEditorIdeInterface()
{
    DEBUG << "NodeEditorIdeInterface object delete";
}

Daqster::QBasePluginObject* NodeEditorIdeInterface::CreatePluginInternal(QObject* Parrent)
{
    NodeEditorIdeObject* Obj = new NodeEditorIdeObject(Parrent);
    if (nullptr != Obj) {
        Obj->SetName(m_PluginDescryptor.GetProperty(PLUGIN_NAME).toString());
    }
    return Obj;
}
