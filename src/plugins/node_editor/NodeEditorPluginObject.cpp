#include "NodeEditorPluginObject.h"
#include "QPluginManager.h"
#include "debug.h"
#include <QMainWindow>
#include<QLabel>
#include<QLayout>
#include<QPushButton>



#include <nodes/NodeData>
#include <nodes/FlowScene>
#include <nodes/FlowView>
#include <nodes/ConnectionStyle>
#include <nodes/TypeConverter>

#include <QtWidgets/QApplication>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QMenuBar>

#include <nodes/DataModelRegistry>

#include "ModuloModel.h"
#include "NumberSourceDataModel.h"
#include "NumberDisplayDataModel.h"
#include "Converters.h"

using QtNodes::DataModelRegistry;
using QtNodes::FlowScene;

using QtNodes::FlowView;
using QtNodes::ConnectionStyle;
using QtNodes::TypeConverter;
using QtNodes::TypeConverterId;

static std::shared_ptr<DataModelRegistry>
registerDataModels()
{
  auto ret = std::make_shared<DataModelRegistry>();


  ret->registerModel<NumberSourceDataModel>("Sources");

  ret->registerModel<NumberDisplayDataModel>("Displays");

//  ret->registerModel<AdditionModel>("Operators");

//  ret->registerModel<SubtractionModel>("Operators");

//  ret->registerModel<MultiplicationModel>("Operators");

//  ret->registerModel<DivisionModel>("Operators");

  ret->registerModel<ModuloModel<int>>("Operators");
  ret->registerModel<ModuloModel<double>>("Operators");


  ret->registerTypeConverter(std::make_pair(ComplexType<int>().type(),
                                            ComplexType<double>().type()),
                             TypeConverter{AnyToAnyComplexIntConverter<int, double>()});

  ret->registerTypeConverter(std::make_pair(ComplexType<double>().type(),
                                            ComplexType<int>().type()),
                             TypeConverter{AnyToAnyComplexIntConverter<double,int>()});

  ret->registerTypeConverter(std::make_pair(ComplexType<int>().type(),
                                            DecimalData().type()),
                             TypeConverter{ComplexIntToDecimalConverter()});
  /*
   *
  ret->registerTypeConverter(std::make_pair(DecimalData().type(),
                                            IntegerData().type()),
                             TypeConverter{DecimalToIntegerConverter()});





  ret->registerTypeConverter(std::make_pair(IntegerData().type(),
                                            DecimalData().type()),
                             TypeConverter{IntegerToDecimalConverter()});


  ret->registerTypeConverter(std::make_pair(DecimalData().type(),
                                            ComplexType<int>().type()),
                             TypeConverter{DecimalToComplexIntConverter()});


  ret->registerTypeConverter(std::make_pair(ComplexType<int>().type(),
                                            DecimalData().type()),
                             TypeConverter{ComplexIntToDecimalConverter()});*/

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
    if( m_Win )
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

    QLabel* label = new QLabel( );
    label->setText("Node Editor");
    QPushButton* button = new QPushButton(m_Win);
    l->addWidget(label);
    l->addWidget(button);
    setStyle();

    auto scene = new FlowScene(registerDataModels(), mainWidget);
    l->addWidget(new FlowView(scene));
    l->setContentsMargins(0, 0, 0, 0);
    l->setSpacing(0);
    m_Win->resize(800, 600);
    m_Win->show();
    m_Win->setAttribute(Qt::WA_DeleteOnClose, true);
    connect(scene, SIGNAL(nodeContextMenu(Node&, const QPointF&)) ,this,SLOT(sceneContextMenuEvent(Node&, const QPointF&)) );
    connect(scene, SIGNAL(nodeDoubleClicked(Node&)) ,this,SLOT(nodeDoubleClicked(Node&))  );
    connect( m_Win, SIGNAL(destroyed(QObject*)), this, SLOT(MainWinDestroyed(QObject*)) );
    connect( button, SIGNAL(clicked(bool)), this, SLOT(ShowPlugins()) );
    return true;
}

void NodeEditorPluginObject::nodeDoubleClicked(Node& n)
{
    QMenu menu;
    QAction *removeAction = menu.addAction("Laa");
    QAction *markAction = menu.addAction("Daa");

    QAction *selectedAction = menu.exec( );
    if( selectedAction == markAction){
        qDebug()<< "Laa";
    }else if( selectedAction == removeAction){
        qDebug()<< "Daa";
    }
}

void
NodeEditorPluginObject::
sceneContextMenuEvent( Node& node, const QPointF& pos)
{
    QMenu menu;
    QAction *embedAction    = menu.addAction("Embed");
    QAction *deembedAction  = menu.addAction("Deembed");
    QGraphicsView *v = node.nodeGraphicsObject().scene()->views().first();
    QPoint viewP = v->mapFromScene(pos);
    QAction *selectedAction = menu.exec( v->viewport()->mapToGlobal(viewP) );
    if(  selectedAction == embedAction  ){
        node.nodeGraphicsObject().embedQWidget(true);
    }
    else if(selectedAction == deembedAction){
        node.nodeGraphicsObject().embedQWidget(false);
    }
}

void NodeEditorPluginObject::DeInitialize()
{
    if( m_Win ){
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
