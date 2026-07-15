#include "NodeEditorAppObject.h"
#include "NodeEditorWidget.h"
#include "QPluginManager.h"
#include "debug.h"

#include <QMainWindow>
#include <QLabel>
#include <QLayout>
#include <QPushButton>
#include <QMenu>

#include <QtNodes/NodeDelegateModelRegistry>
#include <QtNodes/ConnectionStyle>

#include <QtWidgets/QVBoxLayout>

#include "ModuloModel.h"
#include "NumberSourceDataModel.h"
#include "NumberDisplayDataModel.h"
#include "AudioSourceDataModel.h"
#include "QDevIoDisplayModel.h"
#include "Converters.h"
#include "LLamaModelDataModel.h"
#include "ConsoleDataModel.h"

static void registerDefaultNodes(NodeEditorWidget* widget)
{
    auto* registry = widget->getInjectedRegistry();

    registry->registerModel<NumberSourceDataModel>("Sources");
    registry->registerModel<AudioSourceDataModel>("Sources");

    registry->registerModel<NumberDisplayDataModel>("Displays");
    registry->registerModel<QDevIoDisplayModel>("Displays");

    registry->registerModel<ModuloModel<int>>("Operators");
    registry->registerModel<ModuloModel<double>>("Operators");

    registry->registerModel<LLamaModelDataModel>("LLaMA");
    registry->registerModel<ConsoleDataModel>("LLaMA");
}

static void setStyle()
{
    QtNodes::ConnectionStyle::setConnectionStyle(
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

NodeEditorAppObject::NodeEditorAppObject(QObject* Parent)
    : QBasePluginObject(Parent)
    , m_Win(nullptr)
    , m_Widget(nullptr)
{
}

NodeEditorAppObject::~NodeEditorAppObject()
{
    DeInitialize();
}

void NodeEditorAppObject::SetName(const QString& name)
{
    if (nullptr != m_Win) {
        m_Win->setWindowTitle(name);
    }
}

bool NodeEditorAppObject::Initialize()
{
    m_Win = new QMainWindow();
    QWidget* mainWidget = new QWidget(m_Win);
    m_Win->setCentralWidget(mainWidget);
    QVBoxLayout* l = new QVBoxLayout(mainWidget);

    QLabel* label = new QLabel();
    label->setText("Node Editor");
    QPushButton* button = new QPushButton(m_Win);
    l->addWidget(label);
    l->addWidget(button);

    setStyle();

    m_Widget = new NodeEditorWidget(mainWidget);
    registerDefaultNodes(m_Widget);
    m_Widget->buildCanvas();

    l->addWidget(m_Widget);
    l->setContentsMargins(0, 0, 0, 0);
    l->setSpacing(0);

    m_Win->resize(1024, 768);
    m_Win->show();
    m_Win->setAttribute(Qt::WA_DeleteOnClose, true);

    connect(m_Widget, &NodeEditorWidget::nodeDoubleClicked,
            this, &NodeEditorAppObject::nodeDoubleClicked);
    connect(m_Win, SIGNAL(destroyed(QObject*)), this, SLOT(MainWinDestroyed(QObject*)));
    connect(button, SIGNAL(clicked(bool)), this, SLOT(ShowPlugins()));
    return true;
}

void NodeEditorAppObject::nodeDoubleClicked(QtNodes::NodeId nodeId)
{
    Q_UNUSED(nodeId);
    QMenu menu;
    QAction* removeAction = menu.addAction("Laa");
    QAction* markAction = menu.addAction("Daa");

    QAction* selectedAction = menu.exec();
    if (selectedAction == markAction) {
        qDebug() << "Laa";
    } else if (selectedAction == removeAction) {
        qDebug() << "Daa";
    }
}

void NodeEditorAppObject::DeInitialize()
{
    if (nullptr != m_Win) {
        m_Win->deleteLater();
    }
    DEBUG_V << "NodeEditorAppObject destroyed";
}

void NodeEditorAppObject::MainWinDestroyed(QObject* obj)
{
    m_Win = nullptr;
    m_Widget = nullptr;
    deleteLater();
    if (nullptr == obj)
        DEBUG << "Strange::!!!";
}

void NodeEditorAppObject::ShowPlugins()
{
    Daqster::QPluginManager* pm = Daqster::QPluginManager::instance();
    if (nullptr != pm) {
        DEBUG << "Plugin Manager: " << pm;
        pm->ShowPluginManagerGui(m_Win);
    }
}
