#include "AppToolbar.h"
#include<QAction>
#include<QIcon>
#include<QDebug>
#include<QMenu>
#include<QToolButton>
#include"ApplicationsManager.h"

AppToolbar::AppToolbar(QWidget *parent) :
    QToolBar(parent),m_AppMenu(NULL)
{
    QList<Daqster::PluginDescription> list = GetAppPluginList();
    QAction* actionNew = NULL;
    //actionNew->setObjectName( val.GetProperty(PLUGIN_NAME).toString() );
    //actionNew->setIcon(val.GetIcon());
 //   addAction(actionNew);
    QToolButton* tButton = new QToolButton(this);
    QIcon icon;
    icon.addFile(QStringLiteral(":/toolbar/icons/icons/exit.png"), QSize(), QIcon::Normal, QIcon::Off);
    tButton->setIcon(icon);
    m_AppMenu = new QMenu();
    tButton->setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Expanding);
    tButton->setText("Exit");
    tButton->setMenu(m_AppMenu);
    tButton->setPopupMode(QToolButton::MenuButtonPopup);
    tButton->setToolButtonStyle( Qt::ToolButtonTextUnderIcon);
    addWidget(tButton);
    foreach (Daqster::PluginDescription val, list) {
        actionNew = new QAction( val.GetProperty(PLUGIN_NAME).toString(),this);
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
    connect( this,
             SIGNAL(PleaseRunApplication(QString,QStringList,QProcess::OpenMode)),
             &ApplicationsManager::Instance(),
             SLOT(StartApplication(QString,QStringList,QProcess::OpenMode))
             );

    connect( &ApplicationsManager::Instance(),
             SIGNAL(ApplicationEvent(ApplicationsManager::AppHndl_t,ApplicationsManager::AppEvent_t)),
             this,
             SLOT(ApplicationEvent(ApplicationsManager::AppHndl_t,ApplicationsManager::AppEvent_t))
             );

    connect( tButton,
             SIGNAL(clicked(bool)),
             &ApplicationsManager::Instance(),
             SLOT(KillAll())
             );

    ApplicationsManager::Instance().setParent( this );
}

AppToolbar::~AppToolbar()
{

}

void AppToolbar::ApplicationEvent(const ApplicationsManager::AppHndl_t ApHndl, const ApplicationsManager::AppEvent_t &ev )
{
    qDebug() <<  "App " << ApHndl << " Event" << ev;
    ApplicationsManager::AppDescriptor_t Desc;
    if( ApplicationsManager::Instance().GetAppDescryptor( ApHndl , Desc) ){
        switch (ev) {
        case ApplicationsManager::APP_STARTED:{
            QString appName = Desc.Arguments[0];
            QAction* actionNew = new QAction(appName ,this);//TODO: Fix name to be the Argument list
            actionNew->setData( appName );
            Daqster::PluginDescription val;
            if( GetAppPluginDescription( appName ,val)){
                actionNew->setIcon(val.GetIcon());
            }
            m_AppMenu->addAction(actionNew);
            break;
        }
        case ApplicationsManager::APP_STOPED:{

            break;
        }
        default:{
            break;
        }
        }
    }
    else{
        //assert(0);/*TODO: Some smislen error*/
    }
}

bool AppToolbar::GetAppPluginDescription(const QString& Name, Daqster::PluginDescription &Desc ){
    bool Ret = false;
    QList<Daqster::PluginDescription>list = GetAppPluginList();
    foreach (auto pl, list) {
        if( 0 == pl.GetProperty(PLUGIN_NAME).toString().compare(Name) ){
            Desc = pl;
            Ret = true;
            break;
        }
    }
    return Ret;
}

QList<Daqster::PluginDescription> AppToolbar::GetAppPluginList()
{
    Daqster::PluginFilter Filter;
    Filter.AddFilter( PLUGIN_TYPE, QString("%1").arg(Daqster::PluginDescription::APPLICATION_PLUGIN) );
    QList<Daqster::PluginDescription> list = Daqster::QPluginManager::instance()->GetPluginList ( Filter );
    return list;
}

void AppToolbar::OnActionTrigered()
{
  QAction* sender = dynamic_cast<QAction*>( QObject::sender() );
  if( NULL != sender ){
      QString AppName = sender->objectName();
      emit PleaseRunApplication( QString("./Daqster"), QStringList( AppName ) );
      qDebug() << "Run Application: " << AppName;
  }
}
