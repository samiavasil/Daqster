#include "plugindetails.h"
#include "ui_plugindetails.h"

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
