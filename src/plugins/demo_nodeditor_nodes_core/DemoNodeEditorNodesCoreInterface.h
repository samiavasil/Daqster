#ifndef DEMONODEEDITORNODESCOREINTERFACE_H
#define DEMONODEEDITORNODESCOREINTERFACE_H

#include <DaqsterCore/plugin/QPluginInterface.h>

namespace Daqster {

class DemoNodeEditorNodesCoreInterface : public QPluginInterface
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "Daqster.PlugIn.QPluginInterface" FILE "DemoNodeEditorNodesCoreInterface.json")

public:
    explicit DemoNodeEditorNodesCoreInterface(QObject* parent = nullptr);
    ~DemoNodeEditorNodesCoreInterface() override;

protected:
    QBasePluginObject* CreatePluginInternal(QObject* parent = nullptr) override;
};

} // namespace Daqster

#endif // DEMONODEEDITORNODESCOREINTERFACE_H