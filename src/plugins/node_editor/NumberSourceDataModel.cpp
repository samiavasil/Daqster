#include "NumberSourceDataModel.h"

#include <QtCore/QJsonValue>
#include <QtGui/QDoubleValidator>

#include "NumbeSourceDataUi.h"

#include<QTimer>
#include<QRandomGenerator>
#include<QSlider>
NumberSourceDataModel::
NumberSourceDataModel()
  : m_ui(new NumbeSourceDataUi()),m_time(0)
{
  QLineEdit& edit =  m_ui->lineEdit();
  edit.setValidator(new QDoubleValidator());

  edit.setMaximumSize(edit.sizeHint());

  connect(&edit, &QLineEdit::textChanged,
          this, &NumberSourceDataModel::onTextEdited);
  connect( &m_ui->timeSlider(), SIGNAL(valueChanged(int)),
          this, SLOT(ChangeTime(int)));

  edit.setText("0.0");
}

void
NumberSourceDataModel::
ChangeTime(int t){
  m_time = t;
}

QJsonObject
NumberSourceDataModel::
save() const
{
  QJsonObject modelJson = NodeDataModel::save();

  if (_number)
    modelJson["number"] = QString::number(_number->number());

  return modelJson;
}


void
NumberSourceDataModel::
restore(QJsonObject const &p)
{
  QJsonValue v = p["number"];

  if (!v.isUndefined())
  {
    QString strNum = v.toString();
    QLineEdit& edit =  m_ui->lineEdit();
    bool   ok;
    double d = strNum.toDouble(&ok);
    if (ok)
    {
      _number = std::make_shared<ComplexType<double>>(d);
      edit.setText(strNum);
    }
  }
}


unsigned int
NumberSourceDataModel::
nPorts(PortType portType) const
{
  unsigned int result = 1;

  switch (portType)
  {
    case PortType::In:
      result = 0;
      break;

    case PortType::Out:
      result = 1;

    default:
      break;
  }

  return result;
}


void
NumberSourceDataModel::
onTextEdited(QString const &string)
{
  Q_UNUSED(string);

  bool ok = false;
  QLineEdit& edit =  m_ui->lineEdit();
  double number = edit.text().toDouble(&ok);

  if (ok)
  {
    _number = std::make_shared<ComplexType<double>>(number);

    Q_EMIT dataUpdated(0);
  }
  else
  {
    Q_EMIT dataInvalidated(0);
  }
}


NodeDataType
NumberSourceDataModel::
dataType(PortType, PortIndex) const
{
  return ComplexType<double>().type();
}

QWidget *
NumberSourceDataModel::
embeddedWidget()
{
    return m_ui;
}

std::shared_ptr<NodeData>
NumberSourceDataModel::
outData(PortIndex)
{
    QTimer::singleShot( m_time,  Qt::PreciseTimer, this, [this](){
        QLineEdit& edit =  this->m_ui->lineEdit();
        edit.setText(QString::number(QRandomGenerator::global()->bounded(10.2) + 1));
    } );
    return _number;
}
