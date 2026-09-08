#include "ModuloModel.h"
#include <QtGui/QDoubleValidator>
#include <cmath>

using QtNodes::PortType;
using QtNodes::PortIndex;
using QtNodes::NodeData;
using QtNodes::NodeDataType;
using QtNodes::NodeValidationState;

static inline int mod(int a, int b) { return (a % b); }
static inline double mod(double a, double b) { return fmod(a, b); }

ModuloModel::ModuloModel()
{
    m_typeCombo = new QComboBox();
    m_typeCombo->addItem("int");
    m_typeCombo->addItem("double");
    connect(m_typeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &ModuloModel::onTypeChanged);
}

ModuloModel::~ModuloModel() {}

QJsonObject ModuloModel::save() const
{
    QJsonObject modelJson;
    modelJson["name"] = name();
    modelJson["type"] = (m_currentType == DataType::Int) ? "int" : "double";
    return modelJson;
}

void ModuloModel::load(QJsonObject const &p)
{
    QString typeStr = p["type"].toString();
    if (typeStr == "double") {
        m_typeCombo->setCurrentIndex(1);
    } else {
        m_typeCombo->setCurrentIndex(0);
    }
}

unsigned int ModuloModel::nPorts(PortType portType) const
{
    switch (portType) {
    case PortType::In:  return 2;
    case PortType::Out: return 1;
    default: break;
    }
    return 0;
}

NodeDataType ModuloModel::dataType(PortType, PortIndex) const
{
    if (m_currentType == DataType::Int)
        return NumericType<int>().type();
    else
        return NumericType<double>().type();
}

std::shared_ptr<NodeData> ModuloModel::outData(PortIndex)
{
    if (m_currentType == DataType::Int)
        return m_result_int;
    else
        return m_result_dbl;
}

QWidget* ModuloModel::embeddedWidget()
{
    return m_typeCombo;
}

void ModuloModel::onTypeChanged(int index)
{
    DataType newType = (index == 0) ? DataType::Int : DataType::Double;
    if (newType == m_currentType) return;
    switchType(newType);
}

void ModuloModel::switchType(DataType newType)
{
    Q_EMIT portsAboutToBeDeleted(PortType::In, 0, nPorts(PortType::In) - 1);
    Q_EMIT portsAboutToBeDeleted(PortType::Out, 0, nPorts(PortType::Out) - 1);

    m_num1_int.reset();
    m_num2_int.reset();
    m_result_int.reset();
    m_num1_dbl.reset();
    m_num2_dbl.reset();
    m_result_dbl.reset();

    m_currentType = newType;

    Q_EMIT portsDeleted();
    Q_EMIT portsAboutToBeInserted(PortType::In, 0, nPorts(PortType::In) - 1);
    Q_EMIT portsAboutToBeInserted(PortType::Out, 0, nPorts(PortType::Out) - 1);
    Q_EMIT portsInserted();

    NodeValidationState s;
    s._state = NodeValidationState::State::Warning;
    s._stateMessage = m_validationError;
    setValidationState(s);
}

void ModuloModel::recompute()
{
    PortIndex const outPortIndex = 0;

    if (m_currentType == DataType::Int) {
        auto n1 = m_num1_int.lock();
        auto n2 = m_num2_int.lock();

        if (n2 && (n2->number() == 0)) {
            NodeValidationState s;
            s._state = NodeValidationState::State::Error;
            s._stateMessage = QStringLiteral("Division by zero error");
            setValidationState(s);
            m_result_int.reset();
        } else if (n1 && n2) {
            NodeValidationState s;
            s._state = NodeValidationState::State::Valid;
            setValidationState(s);
            m_result_int = std::make_shared<NumericType<int>>(mod(n1->number(), n2->number()));
        } else {
            NodeValidationState s;
            s._state = NodeValidationState::State::Warning;
            s._stateMessage = m_validationError;
            setValidationState(s);
            m_result_int.reset();
        }
    } else {
        auto n1 = m_num1_dbl.lock();
        auto n2 = m_num2_dbl.lock();

        if (n2 && (n2->number() == 0.0)) {
            NodeValidationState s;
            s._state = NodeValidationState::State::Error;
            s._stateMessage = QStringLiteral("Division by zero error");
            setValidationState(s);
            m_result_dbl.reset();
        } else if (n1 && n2) {
            NodeValidationState s;
            s._state = NodeValidationState::State::Valid;
            setValidationState(s);
            m_result_dbl = std::make_shared<NumericType<double>>(mod(n1->number(), n2->number()));
        } else {
            NodeValidationState s;
            s._state = NodeValidationState::State::Warning;
            s._stateMessage = m_validationError;
            setValidationState(s);
            m_result_dbl.reset();
        }
    }

    Q_EMIT dataUpdated(outPortIndex);
}

void ModuloModel::setInData(std::shared_ptr<NodeData> data, PortIndex portIndex)
{
    if (m_currentType == DataType::Int) {
        auto numData = std::dynamic_pointer_cast<NumericType<int>>(data);
        if (portIndex == 0) m_num1_int = numData;
        else m_num2_int = numData;
    } else {
        auto numData = std::dynamic_pointer_cast<NumericType<double>>(data);
        if (portIndex == 0) m_num1_dbl = numData;
        else m_num2_dbl = numData;
    }
    recompute();
}
