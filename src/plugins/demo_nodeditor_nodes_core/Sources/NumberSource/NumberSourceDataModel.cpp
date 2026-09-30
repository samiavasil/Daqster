#include "NumberSourceDataModel.h"
#include <QtCore/QJsonValue>
#include <QTimer>
#include <QRandomGenerator>

using QtNodes::PortType;
using QtNodes::PortIndex;
using QtNodes::NodeData;
using QtNodes::NodeDataType;

NumberSourceDataModel::NumberSourceDataModel()
{
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
    modelJson["randomEnabled"] = m_randomEnabled;
    modelJson["interval"] = m_interval;

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
        switchType(DataType::Int);
    } else {
        switchType(DataType::Double);
    }

    if (p.contains("interval")) {
        onIntervalChanged(p["interval"].toInt());
    }
    if (p.contains("randomEnabled")) {
        onRandomToggled(p["randomEnabled"].toBool());
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
            m_text = strNum;
            Q_EMIT textChanged(m_text);
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

    onTextEdited(m_text);
}

void NumberSourceDataModel::onTextEdited(QString const &string)
{
    m_text = string;

    if (m_randomEnabled)
        return;

    bool ok = false;
    double number = string.toDouble(&ok);

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
    m_randomEnabled = checked;
    Q_EMIT textEditableChanged(!checked);
    updateTimer();
}

void NumberSourceDataModel::onIntervalChanged(int value)
{
    m_interval = value;
    updateTimer();
}

void NumberSourceDataModel::updateTimer()
{
    if (m_randomEnabled && m_interval > 0) {
        m_timer->start(m_interval);
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
        m_text = QString::number(ival);
    } else {
        m_number_dbl = std::make_shared<NumericType<double>>(val);
        m_text = QString::number(val, 'f', 2);
    }

    Q_EMIT textChanged(m_text);
    Q_EMIT dataUpdated(0);
}
