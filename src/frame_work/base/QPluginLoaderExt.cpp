#include "QPluginLoaderExt.h"
#include "debug.h"
QPluginLoaderExt::QPluginLoaderExt(const QString &fileName, QObject *parent):QPluginLoader( fileName, parent ){

}

QPluginLoaderExt::~QPluginLoaderExt(){
    DEBUG_V << "Destroy QPluginLoaderExt '" << fileName() << "'";
    if( isLoaded() ){

        if( unload() ){
            DEBUG_V << "Success unload Plugin library '" << fileName() << "'";
        }
        else{
            DEBUG << "Failed to unload Plugin library '" << fileName() << "'";
        }
    }
}
