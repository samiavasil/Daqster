#include "NodeEditorPluginObject.h"
#include "QPluginManager.h"
#include "debug.h"
#include <QMainWindow>
#include <QLabel>
#include <QLayout>
#include <QPushButton>

#include <QtNodes/NodeData>
#include <QtNodes/BasicGraphicsScene>
#include <QtNodes/DataFlowGraphicsScene>
#include <QtNodes/GraphicsView>
#include <QtNodes/ConnectionStyle>
#include <QtNodes/DataFlowGraphModel>
#include <QtNodes/NodeDelegateModelRegistry>

#include <QtWidgets/QApplication>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QMenuBar>

#include "ModuloModel.h"
#include "NumberSourceDataModel.h"
#include "NumberDisplayDataModel.h"
#include "AudioSourceDataModel.h"
#include "QDevIoDisplayModel.h"
#include "Converters.h"
#include "LLamaModelDataModel.h"
#include "ConsoleDataModel.h"
#include "ChatGraphModel.h"

using QtNodes::NodeDelegateModelRegistry;
using QtNodes::BasicGraphicsScene;
using QtNodes::DataFlowGraphModel;
using QtNodes::DataFlowGraphicsScene;
using QtNodes::GraphicsView;
using QtNodes::ConnectionStyle;

static std::shared_ptr<NodeDelegateModelRegistry>
registerDataModels()
{
    auto ret = std::make_shared<NodeDelegateModelRegistry>();

    ret->registerModel<NumberSourceDataModel>("Sources");
    ret->registerModel<AudioSourceDataModel>("Sources");

    ret->registerModel<NumberDisplayDataModel>("Displays");
    ret->registerModel<QDevIoDisplayModel>("Displays");

    ret->registerModel<ModuloModel<int>>("Operators");
    ret->registerModel<ModuloModel<double>>("Operators");

    ret->registerModel<LLamaModelDataModel>("LLaMA");
    ret->registerModel<ConsoleDataModel>("LLaMA");

    return ret;
}



static
void
setStyle()
{
    ConnectionStyle::setConnectionStyle(
                R"(
                {
                "ConnectionStyle": {
                "ConstructionColor": "gray",
                "NormalColor": "black",
                "SelectedColor": "gray",
                "SelectedHaloColor": "deepskyblue",
                "HoveredColor": "deepskyblue",

                "LineWidth": 3.0,
                "ConstructionLineWidth": 2.0,
                "PointDiameter": 10.0,

                "UseDataDefinedColors": true
                }
                }
                )");
}
















NodeEditorPluginObject::NodeEditorPluginObject(QObject *Parent):QBasePluginObject ( Parent  ),m_Win(nullptr){

}

NodeEditorPluginObject::~NodeEditorPluginObject()
{
    DeInitialize();
}

void NodeEditorPluginObject::SetName(const QString &name)
{
    if( nullptr != m_Win )
    {
        m_Win->setWindowTitle( name );
    }
}

bool NodeEditorPluginObject::Initialize()
{
    m_Win = new QMainWindow();
    QWidget* mainWidget = new QWidget(m_Win);
    m_Win->setCentralWidget(mainWidget);
    QVBoxLayout *l = new QVBoxLayout(mainWidget);

    QLabel* label = new QLabel();
    label->setText("Node Editor");
    QPushButton* button = new QPushButton(m_Win);
    l->addWidget(label);
    l->addWidget(button);
    setStyle();

    auto graphModel = new ChatGraphModel(registerDataModels());
    auto scene = new DataFlowGraphicsScene(*graphModel, mainWidget);
    auto view   = new GraphicsView(scene, mainWidget);
    l->addWidget(view);
    l->setContentsMargins(0, 0, 0, 0);
    l->setSpacing(0);
    m_Win->resize(1024, 768);
    m_Win->show();
    m_Win->setAttribute(Qt::WA_DeleteOnClose, true);
    connect(scene, &BasicGraphicsScene::nodeDoubleClicked,
            this, &NodeEditorPluginObject::nodeDoubleClicked);
    connect(m_Win, SIGNAL(destroyed(QObject*)), this, SLOT(MainWinDestroyed(QObject*)));
    connect(button, SIGNAL(clicked(bool)), this, SLOT(ShowPlugins()));
    return true;
}

void NodeEditorPluginObject::nodeDoubleClicked(NodeId nodeId)
{
    Q_UNUSED(nodeId);
    QMenu menu;
    QAction *removeAction = menu.addAction("Laa");
    QAction *markAction = menu.addAction("Daa");

    QAction *selectedAction = menu.exec();
    if (selectedAction == markAction) {
        qDebug() << "Laa";
    } else if (selectedAction == removeAction) {
        qDebug() << "Daa";
    }
}


void NodeEditorPluginObject::DeInitialize()
{
    if( nullptr != m_Win ){
        m_Win->deleteLater();
    }
    DEBUG_V << "NodeEditorPluginObject destroyed";
}

void NodeEditorPluginObject::MainWinDestroyed( QObject* obj )
{
    m_Win = nullptr;
    deleteLater();
    if( nullptr == obj )
        DEBUG << "Strange::!!!";

}

void NodeEditorPluginObject::ShowPlugins()
{
    Daqster::QPluginManager* pm = Daqster::QPluginManager::instance();
    if( nullptr != pm )
    {
        DEBUG << "Plugin Manager: " << pm;
        //     pm->SearchForPlugins();
        pm->ShowPluginManagerGui( m_Win );
    }
}
