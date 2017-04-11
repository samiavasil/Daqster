#ifndef PLUGINDETAILS_H
#define PLUGINDETAILS_H

#include <QDialog>

namespace Ui {
class PluginDetails;
}

class PluginDetails : public QDialog
{
    Q_OBJECT

public:
    explicit PluginDetails(QWidget *parent = 0);
    ~PluginDetails();

private:
    Ui::PluginDetails *ui;
};

#endif // PLUGINDETAILS_H
