#include "AppToolbar.h"
#include<QApplication>
#include<QAction>
#include<QIcon>
#include<QDebug>
#include"QPluginManager.h"
#include"PluginFilter.h"
AppToolbar::AppToolbar(QWidget *parent) :
    QToolBar(parent)
{
    Daqster::PluginFilter Filter;
    Filter.AddFilter( PLUGIN_TYPE, QString("%1").arg(Daqster::PluginDescription::APPLICATION_PLUGIN) );
    QList<Daqster::PluginDescription> list = Daqster::QPluginManager::instance()->GetPluginList ( Filter );

    foreach (Daqster::PluginDescription val, list) {
        QAction* actionNew = new QAction( val.GetProperty(PLUGIN_NAME).toString(),this);
        actionNew->setObjectName( val.GetProperty(PLUGIN_NAME).toString() );
        actionNew->setIcon(val.GetIcon());
        addAction(actionNew);
        addSeparator();
        connect( actionNew,SIGNAL(triggered(bool)),this,SLOT(OnActionTrigered()) );
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

void AppToolbar::OnActionTrigered()
{
  QAction* sender = dynamic_cast<QAction*>( QObject::sender() );
  if( NULL != sender ){
      QString AppName = sender->objectName();
      emit PleaseRunApplication( AppName );
      qDebug() << "Run Application: " << AppName;
  }
}

