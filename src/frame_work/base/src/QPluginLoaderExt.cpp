#include "QPluginLoaderExt.h"
#include "LogCategories.h"

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
        qCDebug(lcFramework) << "Skipping unload Plugin library during shutdown '" << fileName() << "'";
        return;
    }

    if (!unload()) {
        qCDebug(lcFramework) << "Failed to unload Plugin library '" << fileName() << "'";
        return;
    }

    qCDebug(lcFramework) << "Success unload Plugin library '" << fileName() << "'";
}
