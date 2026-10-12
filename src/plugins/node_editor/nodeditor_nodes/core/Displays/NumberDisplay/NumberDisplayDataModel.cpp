#include "NumberDisplayDataModel.h"

#include "NodeDataTypes/NumericType.h"

using QtNodes::PortType;
using QtNodes::PortIndex;
using QtNodes::NodeData;
using QtNodes::NodeDataType;
using QtNodes::NodeValidationState;

NumberDisplayDataModel::
NumberDisplayDataModel()
{
    // Display nodes must never get a graphics effect (perf): the shadow blur
    // runs per repaint and costs ~46% CPU during video playback.
    QtNodes::NodeStyle s = this->nodeStyle();
    s.ShadowEnabled = false;
    this->setNodeStyle(s);
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
    if (m_currentType == DataType::Int)
        return m_result_int;
    else
        return m_result_dbl;
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
            Q_EMIT displayTextChanged(numberData->numberAsText());
        } else {
            NodeValidationState s;
            s._state = NodeValidationState::State::Warning;
            s._stateMessage = QStringLiteral("Missing or incorrect inputs");
            setValidationState(s);
            Q_EMIT displayTextChanged(QString());
        }
    } else {
        auto numberData = std::dynamic_pointer_cast<NumericType<double>>(data);
        if (numberData) {
            NodeValidationState s;
            s._state = NodeValidationState::State::Valid;
            setValidationState(s);
            Q_EMIT displayTextChanged(numberData->numberAsText());
        } else {
            NodeValidationState s;
            s._state = NodeValidationState::State::Warning;
            s._stateMessage = QStringLiteral("Missing or incorrect inputs");
            setValidationState(s);
            Q_EMIT displayTextChanged(QString());
        }
    }
}

bool
NumberDisplayDataModel::
eventFilter(QObject *object, QEvent *event)
{
    Q_UNUSED(object);
    Q_UNUSED(event);
    return false;
}

QJsonObject NumberDisplayDataModel::save() const
{
    QJsonObject modelJson = NodeDelegateModel::save();
    modelJson["type"] = (m_currentType == DataType::Int) ? "int" : "double";
    return modelJson;
}

void NumberDisplayDataModel::load(QJsonObject const &p)
{
    QString typeStr = p["type"].toString();
    if (typeStr == "int") {
        m_currentType = DataType::Int;
    } else {
        m_currentType = DataType::Double;
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

    m_currentType = newType;

    Q_EMIT portsDeleted();
    Q_EMIT displayTextChanged(QString());
    Q_EMIT portsAboutToBeInserted(PortType::In, 0, 0);
    Q_EMIT portsInserted();
}

QtNodes::NodeValidationState
NumberDisplayDataModel::
validationState() const
{
    return modelValidationState;
}
