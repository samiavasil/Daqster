#ifndef PLUGINDETAILS_H
#define PLUGINDETAILS_H

#include <QDialog>
#include <QString>
#include <memory>
#include "PluginDescription.h"

namespace Ui {
class PluginDetails;
}

namespace Daqster {
class PluginDetails : public QDialog
{
    Q_OBJECT

public:
    explicit PluginDetails( QWidget *parent = 0 );
    ~PluginDetails();

    const Daqster::PluginDescription& PluginDescription() const;

    void setPluginDescription(const Daqster::PluginDescription &Description);

private:
    std::unique_ptr<Ui::PluginDetails> ui;
    Daqster::PluginDescription m_Description;
};
}

#endif // PLUGINDETAILS_H
