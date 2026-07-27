#include "QPluginLoaderExt.h"
#include "debug.h"

bool QPluginLoaderExt::s_isShuttingDown = false;

void QPluginLoaderExt::setShuttingDown(bool v)
{
    s_isShuttingDown = v;
}

QPluginLoaderExt::QPluginLoaderExt(const QString &fileName, QObject *parent):QPluginLoader( fileName, parent ){

}

QPluginLoaderExt::~QPluginLoaderExt(){
    if (!isLoaded()) {
        return;
    }

    if (s_isShuttingDown) {
        DEBUG_V << "Skipping unload Plugin library during shutdown '" << fileName() << "'";
        return;
    }

    if (!unload()) {
        DEBUG << "Failed to unload Plugin library '" << fileName() << "'";
        return;
    }

    DEBUG_V << "Success unload Plugin library '" << fileName() << "'";
}
