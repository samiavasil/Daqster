#pragma once

#include <QPluginInterface.h>

namespace Daqster {

class FrameworkCorePluginInterface : public QPluginInterface
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "org.daqster.QPluginInterface/1.0" FILE "FrameworkCorePluginInterface.json")
    Q_INTERFACES(Daqster::QPluginInterface)

public:
    explicit FrameworkCorePluginInterface(QObject* parent = nullptr);
    ~FrameworkCorePluginInterface() override;

    // QPluginInterface interface
    Daqster::QBasePluginObject* CreatePluginInternal(QObject* parent = nullptr) override;
};

} // namespace Daqster