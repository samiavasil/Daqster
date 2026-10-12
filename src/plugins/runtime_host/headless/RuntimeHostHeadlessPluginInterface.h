#pragma once

#include <QPluginInterface.h>

namespace Daqster {

class RuntimeHostHeadlessPluginInterface : public QPluginInterface
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "org.daqster.QPluginInterface/1.0" FILE "RuntimeHostHeadlessPluginInterface.json")
    Q_INTERFACES(Daqster::QPluginInterface)

public:
    explicit RuntimeHostHeadlessPluginInterface(QObject* parent = nullptr);
    ~RuntimeHostHeadlessPluginInterface() override;

    // QPluginInterface interface
    Daqster::QBasePluginObject* CreatePluginInternal(QObject* parent = nullptr) override;
};

} // namespace Daqster