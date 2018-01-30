#include "AppToolbar.h"
#include<QApplication>
#include<QAction>
#include<QIcon>
#include"QPluginManager.h"

AppToolbar::AppToolbar(QWidget *parent) :
    QToolBar(parent)
{
    QList<Daqster::PluginDescription> list = Daqster::QPluginManager::instance()->GetPluginList ( /*const PluginFilter &Filter = PluginFilter()*/);

    foreach (Daqster::PluginDescription val, list) {
        QAction* actionNew = new QAction( val.GetProperty(PLUGIN_NAME).toString(),this);
        actionNew->setObjectName(QStringLiteral("actionNew"));
        actionNew->setIcon(val.GetIcon());
        addAction(actionNew);
        addSeparator();
    }
    setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
    setSizePolicy(QSizePolicy::Fixed,QSizePolicy::Fixed);
    setWindowFlags(Qt::FramelessWindowHint);
    //dynamic_cast<QApplication*>(QApplication::instance())->desktop();
    move(0,0);
}

AppToolbar::~AppToolbar()
{
}

