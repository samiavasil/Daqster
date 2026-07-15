#pragma once

#include <QObject>
#include "plugin_global.h"
#include "QPluginInterface.h"

using namespace Daqster;

class PLUGIN_EXPORT NodeEditorIdeInterface : public QPluginInterface
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "Daqster.PlugIn.QPluginInterface" FILE "NodeEditorIdeInterface.json")
    Q_INTERFACES(Daqster::QPluginInterface)
public:
    NodeEditorIdeInterface(QObject* parent = nullptr);
    ~NodeEditorIdeInterface();

protected:
    virtual Daqster::QBasePluginObject* CreatePluginInternal(QObject* Parrent = nullptr);
};
