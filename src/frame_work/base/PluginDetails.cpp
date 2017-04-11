#include "PluginDetails.h"
#include "ui_plugindetails.h"
#include <QTextStream>

namespace Daqster{


PluginDetails::PluginDetails(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::PluginDetails)
{
    ui->setupUi(this);
}

PluginDetails::~PluginDetails()
{
    delete ui;
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
    ui->Url->setText( "http://????" );
    ui->Location->setText( m_Description.GetProperty(PLUGIN_LOCATION).toString() );
    ui->Platform->setText( "????" );
    QString s;
    QTextStream out(&s);
    out << "Plugin name: " << m_Description.GetProperty(PLUGIN_NAME).toString() << endl <<
           "Plugin type: " << m_Description.GetProperty(PLUGIN_TYPE).toString() << endl <<
           "Plugin Description: " << m_Description.GetProperty(PLUGIN_DESCRIPTION).toString() << endl <<
           "Plugin Description: " << m_Description.GetProperty(PLUGIN_DETAIL_DESCRIPTION).toString();

    ui->Description->setText( s );
    ui->Copyright->setText( m_Description.GetProperty(PLUGIN_AUTHOR).toString() );
    ui->License->setText( m_Description.GetProperty(PLUGIN_LICENSE).toString() );
    ui->Dependency->setText( "1 2 3 ?" );

//    PLUGIN_DESCRIPTION
//    PLUGIN_TYPE
//    PLUGIN_HASH
//    PLUGIN_HELTHY_STATE
}

}
