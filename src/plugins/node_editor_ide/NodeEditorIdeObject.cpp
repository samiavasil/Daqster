#include "NodeEditorIdeObject.h"
#include "NodeEditorWidget.h"
#include "QPluginManager.h"
#include "INodeProvider.h"
#include "debug.h"

#include <QMainWindow>
#include <QLabel>
#include <QLayout>
#include <QPushButton>
#include <QMenu>

#include <QtNodes/NodeDelegateModelRegistry>
#include <QtNodes/ConnectionStyle>

#include <QtWidgets/QVBoxLayout>

#include "AudioSourceDataModel.h"
#include "QDevIoDisplayModel.h"
#include "LLamaModelDataModel.h"
#include "ConsoleDataModel.h"

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

NodeEditorIdeObject::NodeEditorIdeObject(QObject* Parent)
    : Daqster::QBasePluginObject(Parent)
    , m_Win(nullptr)
    , m_Widget(nullptr)
{
}

NodeEditorIdeObject::~NodeEditorIdeObject()
{
    DeInitialize();
}

void NodeEditorIdeObject::SetName(const QString& name)
{
    if (nullptr != m_Win) {
        m_Win->setWindowTitle(name);
    }
}

bool NodeEditorIdeObject::Initialize()
{
    m_Win = new QMainWindow();
    QWidget* mainWidget = new QWidget(m_Win);
    m_Win->setCentralWidget(mainWidget);
    QVBoxLayout* l = new QVBoxLayout(mainWidget);

    QLabel* label = new QLabel();
    label->setText("Node Editor IDE");
    QPushButton* button = new QPushButton(m_Win);
    l->addWidget(label);
    l->addWidget(button);

    setStyle();

    m_Widget = new NodeEditorWidget(mainWidget);

    // Phase 1: Register built-in nodes (from former node_editor_app)
    registerBuiltInNodes();

    // Phase 2: Discover and register external INodeProvider plugins
    discoverAndRegisterExternalNodes();

    // Build canvas AFTER all nodes are registered
    m_Widget->buildCanvas();

    l->addWidget(m_Widget);
    l->setContentsMargins(0, 0, 0, 0);
    l->setSpacing(0);

    m_Win->resize(1024, 768);
    m_Win->show();
    m_Win->setAttribute(Qt::WA_DeleteOnClose, true);

    connect(m_Widget, &NodeEditorWidget::nodeDoubleClicked,
            this, &NodeEditorIdeObject::nodeDoubleClicked);
    connect(m_Win, SIGNAL(destroyed(QObject*)), this, SLOT(MainWinDestroyed(QObject*)));
    connect(button, SIGNAL(clicked(bool)), this, SLOT(ShowPlugins()));
    return true;
}

void NodeEditorIdeObject::registerBuiltInNodes()
{
    auto* registry = m_Widget->getInjectedRegistry();

    registry->registerModel<AudioSourceDataModel>("Sources");

    registry->registerModel<QDevIoDisplayModel>("Displays");

    registry->registerModel<LLamaModelDataModel>("LLaMA");
    registry->registerModel<ConsoleDataModel>("LLaMA");
}

void NodeEditorIdeObject::discoverAndRegisterExternalNodes()
{
    Daqster::QPluginManager* pm = Daqster::QPluginManager::instance();
    if (!pm) return;

    QObjectList providers = pm->instances(INodeProvider_IID);
    auto* registry = m_Widget->getInjectedRegistry();

    for (QObject* obj : providers) {
        auto* provider = qobject_cast<Daqster::INodeProvider*>(obj);
        if (!provider) continue;

        QString name = obj->property("name").toString();
        DEBUG << "Discovered INodeProvider plugin:" << name;

        provider->registerNodes(*registry);
    }
}

void NodeEditorIdeObject::nodeDoubleClicked(QtNodes::NodeId nodeId)
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

void NodeEditorIdeObject::DeInitialize()
{
    if (nullptr != m_Win) {
        m_Win->deleteLater();
    }
    DEBUG_V << "NodeEditorIdeObject destroyed";
}

void NodeEditorIdeObject::MainWinDestroyed(QObject* obj)
{
    m_Win = nullptr;
    m_Widget = nullptr;
    deleteLater();
    if (nullptr == obj)
        DEBUG << "Strange::!!!";
}

void NodeEditorIdeObject::ShowPlugins()
{
    Daqster::QPluginManager* pm = Daqster::QPluginManager::instance();
    if (nullptr != pm) {
        DEBUG << "Plugin Manager: " << pm;
        pm->ShowPluginManagerGui(m_Win);
    }
}
