#include "PluginDetails.h"
#include "ui_plugindetails.h"
#include <QTextStream>

namespace Daqster{


PluginDetails::PluginDetails(QWidget *parent) :
    QDialog(parent),
    ui(std::make_unique<Ui::PluginDetails>())
{
    ui->setupUi(this);
}

PluginDetails::~PluginDetails()
{
    // ui is automatically cleaned up by std::unique_ptr
}

const PluginDescription &PluginDetails::PluginDescription() const
{
    return m_Description;
}

void PluginDetails::setPluginDescription(const Daqster::PluginDescription &Description )
{
    m_Description = Description;
    ui->Name->setText( m_Description.GetProperty(PLUGIN_NAME).toString() );
    ui->Version->setText( m_Description.GetProperty(PLUGIN_VERSION).toString() );
    ui->Type->setText( m_Description.GetProperty(PLUGIN_TYPE_NAME).toString() );
    ui->Url->setText( m_Description.GetProperty("Url").toString() );
    ui->Location->setText( m_Description.GetProperty(PLUGIN_LOCATION).toString() );
    ui->Platform->setText( m_Description.GetProperty("Platform").toString() );
    QString s;
    QTextStream out(&s);
    out << "Plugin name: " << m_Description.GetProperty(PLUGIN_NAME).toString() << Qt::endl <<
           "Plugin type: " << m_Description.GetProperty(PLUGIN_TYPE).toString() << Qt::endl <<
           "Description: " << m_Description.GetProperty(PLUGIN_DESCRIPTION).toString() << Qt::endl <<
           "Detail: " << m_Description.GetProperty(PLUGIN_DETAIL_DESCRIPTION).toString();

    ui->Description->setText( s );
    ui->Copyright->setText( m_Description.GetProperty(PLUGIN_AUTHOR).toString() );
    ui->License->setText( m_Description.GetProperty(PLUGIN_LICENSE).toString() );
    ui->Dependency->setText( m_Description.GetProperty("Dependency").toString() );
}

}
