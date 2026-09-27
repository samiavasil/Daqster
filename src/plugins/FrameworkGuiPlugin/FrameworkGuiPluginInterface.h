#pragma once

#include <QPluginInterface.h>

namespace Daqster {

class FrameworkGuiPluginInterface : public QPluginInterface
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "org.daqster.QPluginInterface/1.0" FILE "FrameworkGuiPluginInterface.json")
    Q_INTERFACES(Daqster::QPluginInterface)

public:
    explicit FrameworkGuiPluginInterface(QObject* parent = nullptr);
    ~FrameworkGuiPluginInterface() override;

    // QPluginInterface interface
    Daqster::QBasePluginObject* CreatePluginInternal(QObject* parent = nullptr) override;
};

} // namespace Daqster