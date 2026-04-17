#include "QPluginLoaderExt.h"
#include "debug.h"
QPluginLoaderExt::QPluginLoaderExt(const QString &fileName, QObject *parent):QPluginLoader( fileName, parent ){

}

QPluginLoaderExt::~QPluginLoaderExt(){
    DEBUG_V << "Try to destroy QPluginLoaderExt '" << fileName() << "'";
    if (!isLoaded()) {
        DEBUG_V << "Success unload Plugin library [was not loaded] '" << fileName() << "'";
        return;
    }

    if (!unload()) {
        DEBUG << "Failed to unload Plugin library '" << fileName() << "'";
        return;
    }

    DEBUG_V << "Success unload Plugin library '" << fileName() << "'";
}
