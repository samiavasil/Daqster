#pragma once

#include <QObject>
#include "plugin_global.h"
#include "QPluginInterface.h"

using namespace Daqster;

class PLUGIN_EXPORT NodeEditorAppInterface : public QPluginInterface
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "Daqster.PlugIn.QPluginInterface" FILE "NodeEditorAppInterface.json")
    Q_INTERFACES(Daqster::QPluginInterface)
public:
    NodeEditorAppInterface(QObject* parent = nullptr);
    ~NodeEditorAppInterface();

protected:
    virtual Daqster::QBasePluginObject* CreatePluginInternal(QObject* Parrent = nullptr);
};
