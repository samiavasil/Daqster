#include "NodeEditorInterface.h"
#include "debug.h"
#include "NodeEditorPluginObject.h"

NodeEditorInterface::NodeEditorInterface(QObject* parent ):QPluginInterface(parent)
{
    Q_INIT_RESOURCE(node_editor);
    DEBUG << "NodeEditorInterface object create";
    QIcon icon( QString::fromUtf8(":/node_editor.png") );
    m_PluginDescryptor.SetIcon( icon );
    m_PluginDescryptor.SetProperty( PLUGIN_NAME, "NodeEditor" );
    m_PluginDescryptor.SetProperty( PLUGIN_TYPE, Daqster::PluginDescription::APPLICATION_PLUGIN );
    m_PluginDescryptor.SetProperty( PLUGIN_TYPE_NAME, "SOME_TYPE" );
    m_PluginDescryptor.SetProperty( PLUGIN_VERSION, "0.0.1" );
    m_PluginDescryptor.SetProperty( PLUGIN_DESCRIPTION, "Node editor: Visual application factory." );
    char docstr[] = \
            "This is a basic NodeEditor plugin .\n\\n\
            Here you can add detailed description of the plugin...";
            m_PluginDescryptor.SetProperty( PLUGIN_DETAIL_DESCRIPTION, QObject::tr( docstr ) );
    m_PluginDescryptor.SetProperty( PLUGIN_LICENSE, QObject::tr( "The plugin's license have to be....." ) );
    m_PluginDescryptor.SetProperty( PLUGIN_AUTHOR, "Vasil Vasilev" );
}

NodeEditorInterface::~NodeEditorInterface(  )
{
    DEBUG << "NodeEditorInterface object delete";
}

Daqster::QBasePluginObject *NodeEditorInterface::CreatePluginInternal(QObject *Parrent)
{
    NodeEditorPluginObject* Obj = new NodeEditorPluginObject(Parrent);
    if( nullptr != Obj ){
        Obj->SetName( m_PluginDescryptor.GetProperty(PLUGIN_NAME).toString() );
    }
    return Obj;
}

#if QT_VERSION < QT_VERSION_CHECK(5, 0, 0)
Q_EXPORT_PLUGIN2(NodeEditor, NodeEditorInterface)
#endif


