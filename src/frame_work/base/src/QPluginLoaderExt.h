#ifndef QPLUGINLOADEREXT_H
#define QPLUGINLOADEREXT_H
#include<QPluginLoader>

class QPluginLoaderExt:public QPluginLoader
{
    Q_OBJECT
public:
    explicit QPluginLoaderExt( const QString &fileName, QObject *parent = Q_NULLPTR );
    ~QPluginLoaderExt();

    static void setShuttingDown(bool v);

private:
    static bool s_isShuttingDown;
};

#endif // QPLUGINLOADEREXT_H
