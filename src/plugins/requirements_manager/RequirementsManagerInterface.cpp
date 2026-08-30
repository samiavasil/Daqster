#include "RequirementsManagerInterface.h"
#include "debug.h"
#include "RequirementsManagerObject.h"

RequirementsManagerInterface::RequirementsManagerInterface(QObject* parent)
    : Daqster::QPluginInterface(parent)
{
    DEBUG << "RequirementsManagerInterface object create";
    m_PluginDescriptor.SetProperty(PLUGIN_NAME, "RequirementsManager");
    m_PluginDescriptor.SetProperty(PLUGIN_TYPE, Daqster::PluginDescription::APPLICATION_PLUGIN);
    m_PluginDescriptor.SetProperty(PLUGIN_TYPE_NAME, "REQUIREMENTS");
    m_PluginDescriptor.SetProperty(PLUGIN_VERSION, "0.2.0");
    m_PluginDescriptor.SetProperty(PLUGIN_DESCRIPTION, "Requirements Viewer/Editor tool for Markdown-based traceable requirements.");
    char docstr[] =
        "Requirements Manager plugin.\n\n"
        "Opens a DevelopmentProcess/requirements/ directory and provides a tree "
        "view of active and archived requirements with a read-only preview and a "
        "raw Markdown editor.\n"
        "Acceptance criteria checkboxes are written back to the .md files.";
    m_PluginDescriptor.SetProperty(PLUGIN_DETAIL_DESCRIPTION, QObject::tr(docstr));
    m_PluginDescriptor.SetProperty(PLUGIN_LICENSE, QObject::tr("GNU LGPL v2+"));
    m_PluginDescriptor.SetProperty(PLUGIN_AUTHOR, "Vasil Vasilev");
}

RequirementsManagerInterface::~RequirementsManagerInterface()
{
    DEBUG << "RequirementsManagerInterface object delete";
}

Daqster::QBasePluginObject* RequirementsManagerInterface::CreatePluginInternal(QObject* Parrent)
{
    RequirementsManagerObject* Obj = new RequirementsManagerObject(Parrent);
    if (nullptr != Obj) {
        Obj->SetName(m_PluginDescriptor.GetProperty(PLUGIN_NAME).toString());
    }
    return Obj;
}
