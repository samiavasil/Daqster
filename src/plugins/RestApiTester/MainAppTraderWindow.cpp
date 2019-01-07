#include "MainAppTraderWindow.h"
#include "MainAppTraderWindow.h"
#include "debug.h"
#include "QPluginManager.h"
//#include"testplugincreation.h"
#include"ui_mainwindow.h"
#include<QMdiSubWindow>
#include<QMouseEvent>
#include<QPluginLoader>
#include<QPluginManager.h>
//#include"AppToolbar.h"
#include<QBasePluginObject.h>
#include<QTimer>
#include<QCursor>
#include"RestApi.h"
#include"RestApiTester.h"

QTimer timer;

MainAppTraderWindow::MainAppTraderWindow(QWidget *parent) :
    QMainWindow(parent),ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    setAttribute(Qt::WA_QuitOnClose, true );
    setCentralWidget(ui->mdiArea);
    addAction(ui->actionNew);
    addAction(ui->actionSave);
    addAction(ui->actionOpen);
    addAction(ui->actionFullScreen);
    addAction(ui->actionHideMainMenu);
    addAction(ui->actionHideToolbar);
    setMouseTracking(true);
    ui->mainToolBar->setMouseTracking(true);
    ui->mdiArea->setMouseTracking(true);

    connect(&timer,SIGNAL(timeout()),this,SLOT(CursorShow()));
    timer.start(100);
}


MainAppTraderWindow::~MainAppTraderWindow()
{
    if( ui ){
        delete ui;
    }
}

void MainAppTraderWindow::CursorShow(){
    QPoint pos = QCursor::pos();
    QString st = QString("Position: %1,%2 ").arg(pos.x()).arg(pos.y());
    ui->lineEdit->setText( st);
}

void MainAppTraderWindow::onUndoAvailable()
{
    //emit undoAvailable(_designer->core()->formWindowManager()->actionUndo()->isEnabled());
}

void MainAppTraderWindow::onRedoAvailable()
{
    //emit redoAvailable(_designer->core()->formWindowManager()->actionRedo()->isEnabled());
}

void MainAppTraderWindow::onCopyAvailable()
{
    //emit copyAvailable(_designer->core()->formWindowManager()->actionCopy()->isEnabled());
}

void MainAppTraderWindow::onPasteAvailable()
{
    //emit pasteAvailable(_designer->core()->formWindowManager()->actionPaste()->isEnabled());
}

void MainAppTraderWindow::mouseMoveEvent( QMouseEvent * event ){

    if( event ){
        if( !menuBar()->isVisible() ){
            if( ( 10 >= event->pos().y() ) && ( 10 >= event->pos().x() ) ){
                menuBar()->show();
            }
        }
        else{
            int height = menuBar()->size().height();
            if( ( ui->actionHideMainMenu->isChecked() ) && ( height < event->pos().y() )  ){
                menuBar()->hide();
            }
        }
    }
    QMainWindow::mouseMoveEvent(event);
}

void MainAppTraderWindow::on_actionNew_triggered()
{
  Daqster::QPluginManager::instance()->ShowPluginManagerGui(this);
}

void MainAppTraderWindow::on_actionFullScreen_triggered(bool checked)
{
    //static QMdiSubWindow* old;
    if( checked ){
        showFullScreen();
    }
    else{
        showNormal();
    }

}

void MainAppTraderWindow::on_actionHideToolbar_triggered(bool checked)
{
    if( checked ){
        ui->mainToolBar->hide();
        ui->statusBar->hide();
    }
    else{
        ui->mainToolBar->show();
        ui->statusBar->show();
    }
}


void MainAppTraderWindow::on_actionHideMainMenu_triggered(bool checked)
{
    if( checked ){
        menuBar()->hide();
    }
    else{
        menuBar()->show();
    }
}

void MainAppTraderWindow::on_actionSave_triggered()
{
    Daqster::QPluginManager* PluginManager = Daqster::QPluginManager::instance();
    if( NULL != PluginManager )
    {
        qDebug() << "Plugin Manager: " << PluginManager;
      //  PluginManager->SearchForPlugins();
        //PluginManager->ShowPluginManagerGui();
        QList<Daqster::PluginDescription> PluginsList = PluginManager->GetPluginList();
        Daqster::QBasePluginObject* obj;
        foreach ( const Daqster::PluginDescription& Desc, PluginsList) {
           for( int i=0;i < 1; i++){
                obj = PluginManager->CreatePluginObject( Desc.GetProperty(PLUGIN_HASH).toString(), this );
                if( NULL != obj ){
                    obj->Initialize();
                }
           }
        }
    }
}

void MainAppTraderWindow::closeEvent(QCloseEvent *event)
{
        /*if (maybeSave()) {
            writeSettings();
            event->accept();
        } else {
            event->ignore();
        }*/
    if( 1 ){//TODO
        event->accept();
        foreach (QWidget *widget, QApplication::topLevelWidgets()) {
          widget->close();
        }
    }
}

void MainAppTraderWindow::on_actionOpen_triggered()
{
//    AppToolbar* tool = new AppToolbar();
//    connect(tool,SIGNAL(PleaseRunApplication(QString)),this, SLOT(RunApplication(QString)));
//    tool->show();
    RestApiTester dlg(this);
    dlg.setWindowFlags(Qt::Window);
    dlg.exec();
}

void MainAppTraderWindow::RunApplication(const QString& AppName )
{
    qDebug() << "Run Application: " << AppName;
}
