#include "NumberDisplayDataModel.h"

#include "NodeDataTypes/NumericType.h"

using QtNodes::PortType;
using QtNodes::PortIndex;
using QtNodes::NodeData;
using QtNodes::NodeDataType;
using QtNodes::NodeValidationState;

NumberDisplayDataModel::
NumberDisplayDataModel()
    : _label(new QLabel())
{
    _label->setMargin(3);

    m_typeCombo = new QComboBox();
    m_typeCombo->addItem("double");
    m_typeCombo->addItem("int");
    connect(m_typeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &NumberDisplayDataModel::onTypeChanged);

    m_wrapper = new QWidget();
    auto* layout = new QVBoxLayout(m_wrapper);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(2);
    layout->addWidget(m_typeCombo);
    layout->addWidget(_label);
}

unsigned int
NumberDisplayDataModel::
nPorts(PortType portType) const
{
    switch (portType)
    {
    case PortType::In:  return 1;
    case PortType::Out: return 0;
    default: break;
    }
    return 0;
}

NodeDataType
NumberDisplayDataModel::
dataType(PortType, PortIndex) const
{
    if (m_currentType == DataType::Int)
        return NumericType<int>().type();
    else
        return NumericType<double>().type();
}

std::shared_ptr<NodeData>
NumberDisplayDataModel::
outData(PortIndex)
{
    return {};
}

void
NumberDisplayDataModel::
setInData(std::shared_ptr<NodeData> data, PortIndex const)
{
    if (m_currentType == DataType::Int) {
        auto numberData = std::dynamic_pointer_cast<NumericType<int>>(data);
        if (numberData) {
            NodeValidationState s;
            s._state = NodeValidationState::State::Valid;
            setValidationState(s);
            _label->setText(numberData->numberAsText());
        } else {
            NodeValidationState s;
            s._state = NodeValidationState::State::Warning;
            s._stateMessage = QStringLiteral("Missing or incorrect inputs");
            setValidationState(s);
            _label->clear();
        }
    } else {
        auto numberData = std::dynamic_pointer_cast<NumericType<double>>(data);
        if (numberData) {
            NodeValidationState s;
            s._state = NodeValidationState::State::Valid;
            setValidationState(s);
            _label->setText(numberData->numberAsText());
        } else {
            NodeValidationState s;
            s._state = NodeValidationState::State::Warning;
            s._stateMessage = QStringLiteral("Missing or incorrect inputs");
            setValidationState(s);
            _label->clear();
        }
    }

    _label->adjustSize();
}

QJsonObject NumberDisplayDataModel::save() const
{
    QJsonObject modelJson;
    modelJson["name"] = name();
    modelJson["type"] = (m_currentType == DataType::Int) ? "int" : "double";
    return modelJson;
}

void NumberDisplayDataModel::load(QJsonObject const &p)
{
    QString typeStr = p["type"].toString();
    if (typeStr == "int") {
        m_typeCombo->setCurrentIndex(1);
    } else {
        m_typeCombo->setCurrentIndex(0);
    }
}

void NumberDisplayDataModel::onTypeChanged(int index)
{
    DataType newType = (index == 0) ? DataType::Double : DataType::Int;
    if (newType == m_currentType) return;
    switchType(newType);
}

void NumberDisplayDataModel::switchType(DataType newType)
{
    Q_EMIT portsAboutToBeDeleted(PortType::In, 0, 0);

    m_result_int.reset();
    m_result_dbl.reset();
    _label->clear();

    m_currentType = newType;

    Q_EMIT portsDeleted();
    Q_EMIT portsAboutToBeInserted(PortType::In, 0, 0);
    Q_EMIT portsInserted();
}
