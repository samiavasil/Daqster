#include "NumberSourceDataModel.h"
#include "NumberSourceDataUi.h"
#include <QtCore/QJsonValue>
#include <QtGui/QDoubleValidator>
#include <QTimer>
#include <QRandomGenerator>
#include <QCheckBox>
#include <QSpinBox>
#include <QLineEdit>

using QtNodes::PortType;
using QtNodes::PortIndex;
using QtNodes::NodeData;
using QtNodes::NodeDataType;

NumberSourceDataModel::NumberSourceDataModel()
{
    m_ui = new NumberSourceDataUi();

    QLineEdit& edit = m_ui->lineEdit();
    edit.setValidator(new QDoubleValidator(&edit));
    edit.setMaximumSize(edit.sizeHint());

    connect(&edit, &QLineEdit::textChanged, this, &NumberSourceDataModel::onTextEdited);

    edit.setText("0.0");

    // Type selector combo
    m_typeCombo = new QComboBox();
    m_typeCombo->addItem("double");
    m_typeCombo->addItem("int");
    connect(m_typeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &NumberSourceDataModel::onTypeChanged);

    // Wrapper layout: combo on top, original UI below
    m_wrapper = new QWidget();
    auto* layout = new QVBoxLayout(m_wrapper);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(2);
    layout->addWidget(m_typeCombo);
    layout->addWidget(m_ui);

    // Random mode connections
    connect(&m_ui->randomEnabled(), &QCheckBox::toggled,
            this, &NumberSourceDataModel::onRandomToggled);
    connect(&m_ui->intervalSpin(), QOverload<int>::of(&QSpinBox::valueChanged),
            this, &NumberSourceDataModel::onIntervalChanged);

    // Timer for random generation
    m_timer = new QTimer(this);
    m_timer->setSingleShot(false);
    connect(m_timer, &QTimer::timeout, this, &NumberSourceDataModel::onTimerTick);
}

NumberSourceDataModel::~NumberSourceDataModel() {}

QJsonObject NumberSourceDataModel::save() const
{
    QJsonObject modelJson = NodeDelegateModel::save();

    modelJson["type"] = (m_currentType == DataType::Int) ? "int" : "double";
    modelJson["randomEnabled"] = m_ui->randomEnabled().isChecked();
    modelJson["interval"] = m_ui->intervalSpin().value();

    if (m_currentType == DataType::Int && m_number_int)
        modelJson["number"] = QString::number(m_number_int->number());
    else if (m_currentType == DataType::Double && m_number_dbl)
        modelJson["number"] = QString::number(m_number_dbl->number());

    return modelJson;
}

void NumberSourceDataModel::load(QJsonObject const &p)
{
    QString typeStr = p["type"].toString();
    if (typeStr == "int") {
        m_typeCombo->setCurrentIndex(1);
    } else {
        m_typeCombo->setCurrentIndex(0);
    }

    if (p.contains("interval")) {
        m_ui->intervalSpin().setValue(p["interval"].toInt());
    }
    if (p.contains("randomEnabled")) {
        m_ui->randomEnabled().setChecked(p["randomEnabled"].toBool());
    }

    QJsonValue v = p["number"];
    if (!v.isUndefined()) {
        QString strNum = v.toString();
        bool ok;
        double d = strNum.toDouble(&ok);
        if (ok) {
            if (m_currentType == DataType::Int) {
                m_number_int = std::make_shared<NumericType<int>>(static_cast<int>(d));
            } else {
                m_number_dbl = std::make_shared<NumericType<double>>(d);
            }
            m_ui->lineEdit().setText(strNum);
        }
    }
}

unsigned int NumberSourceDataModel::nPorts(PortType portType) const
{
    switch (portType) {
    case PortType::In:  return 1;
    case PortType::Out: return 1;
    default: break;
    }
    return 0;
}

NodeDataType NumberSourceDataModel::dataType(PortType type, PortIndex ind) const
{
    if (ind != 0) return {};

    if (m_currentType == DataType::Int) {
        return NumericType<int>().type();
    } else {
        return NumericType<double>().type();
    }
}

std::shared_ptr<NodeData> NumberSourceDataModel::outData(PortIndex const)
{
    if (m_currentType == DataType::Int)
        return m_number_int;
    else
        return m_number_dbl;
}

void NumberSourceDataModel::setInData(std::shared_ptr<QtNodes::NodeData> data, QtNodes::PortIndex const port)
{
    Q_UNUSED(data);
    Q_UNUSED(port);
}

QWidget* NumberSourceDataModel::embeddedWidget()
{
    return m_wrapper;
}

void NumberSourceDataModel::onTypeChanged(int index)
{
    DataType newType = (index == 0) ? DataType::Double : DataType::Int;
    if (newType == m_currentType) return;
    switchType(newType);
}

void NumberSourceDataModel::switchType(DataType newType)
{
    Q_EMIT portsAboutToBeDeleted(PortType::In, 0, 0);
    Q_EMIT portsAboutToBeDeleted(PortType::Out, 0, 0);

    m_number_int.reset();
    m_number_dbl.reset();

    m_currentType = newType;

    Q_EMIT portsDeleted();
    Q_EMIT portsAboutToBeInserted(PortType::In, 0, 0);
    Q_EMIT portsAboutToBeInserted(PortType::Out, 0, 0);
    Q_EMIT portsInserted();

    onTextEdited(m_ui->lineEdit().text());
}

void NumberSourceDataModel::onTextEdited(QString const &string)
{
    Q_UNUSED(string);

    if (m_ui->randomEnabled().isChecked())
        return;

    bool ok = false;
    double number = m_ui->lineEdit().text().toDouble(&ok);

    if (ok) {
        if (m_currentType == DataType::Int) {
            m_number_int = std::make_shared<NumericType<int>>(static_cast<int>(number));
        } else {
            m_number_dbl = std::make_shared<NumericType<double>>(number);
        }
        Q_EMIT dataUpdated(0);
    } else {
        Q_EMIT dataInvalidated(0);
    }
}

void NumberSourceDataModel::onRandomToggled(bool checked)
{
    m_ui->lineEdit().setEnabled(!checked);
    updateTimer();
}

void NumberSourceDataModel::onIntervalChanged(int value)
{
    Q_UNUSED(value);
    updateTimer();
}

void NumberSourceDataModel::updateTimer()
{
    if (m_ui->randomEnabled().isChecked() && m_ui->intervalSpin().value() > 0) {
        m_timer->start(m_ui->intervalSpin().value());
    } else {
        m_timer->stop();
    }
}

void NumberSourceDataModel::onTimerTick()
{
    generateRandom();
}

void NumberSourceDataModel::generateRandom()
{
    double val = QRandomGenerator::global()->bounded(100) + 1;

    if (m_currentType == DataType::Int) {
        int ival = static_cast<int>(val);
        m_number_int = std::make_shared<NumericType<int>>(ival);
        m_ui->lineEdit().setText(QString::number(ival));
    } else {
        m_number_dbl = std::make_shared<NumericType<double>>(val);
        m_ui->lineEdit().setText(QString::number(val, 'f', 2));
    }

    Q_EMIT dataUpdated(0);
}
