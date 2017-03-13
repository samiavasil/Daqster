#include "DaqsterTeplateInterface.h"
#include "base/debug.h"

TemplatePluginObject::TemplatePluginObject(QObject *Parent):QBasePluginObject ( Parent  ){
    m_Win = new QMainWindow();
    m_Win->show();
    m_Win->setAttribute(Qt::WA_DeleteOnClose, true);
    connect( m_Win, SIGNAL(destroyed(QObject*)), this, SLOT(MainWinDestroyed(QObject*)) );
}

TemplatePluginObject::~TemplatePluginObject()
{
    qDebug() << "TemplatePluginObject destroyed";
}

void TemplatePluginObject::MainWinDestroyed( QObject* obj )
{
    m_Win = NULL;
    deleteLater();
    if( NULL == obj )
        qDebug() << "Strange::!!!";

}

DaqsterTeplateInterface::DaqsterTeplateInterface(QObject* parent ):QPluginBaseInterface(parent)
{
    DEBUG << "QwtPlotWorkInterface object create";
    m_Icon.addFile( QString::fromUtf8(":/template.png") );
    m_Name            = "PluginTemplate";
    m_PluginType      = SOME_TYPE;
    m_PluginTypeName  = "SOME_TYPE";
    m_Version         = "0.0.1";
    m_Description     = "MyPluginTemplate";
    char docstr[] = \
    "This is a basic Daqster plugin template and can be used for implementing a new type daqster plugin \n\
    \n\
    Here you can add detailed description of the plugin...";
    m_DetailDescription = QObject::tr( docstr );
    m_License = QObject::tr("The plugin's license have to be.....");;
    m_Author = "Plugin Author";
}

DaqsterTeplateInterface::~DaqsterTeplateInterface(  )
{
    DEBUG << "QwtPlotWorkInterface object delete";
}

Daqster::QBasePluginObject *DaqsterTeplateInterface::createPluginInternal(QObject *Parrent)
{
    return new TemplatePluginObject(Parrent);
}

#if QT_VERSION < QT_VERSION_CHECK(5, 0, 0)
Q_EXPORT_PLUGIN2(DaqsterTemlatePlugin, DaqsterTeplateInterface)
#endif


