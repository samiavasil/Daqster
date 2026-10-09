#pragma once

#include <QPluginInterface.h>

namespace Daqster {

class RuntimeHostGuiPluginInterface : public QPluginInterface
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "org.daqster.QPluginInterface/1.0" FILE "RuntimeHostGuiPluginInterface.json")
    Q_INTERFACES(Daqster::QPluginInterface)

public:
    explicit RuntimeHostGuiPluginInterface(QObject* parent = nullptr);
    ~RuntimeHostGuiPluginInterface() override;

    // QPluginInterface interface
    Daqster::QBasePluginObject* CreatePluginInternal(QObject* parent = nullptr) override;
};

} // namespace Daqster