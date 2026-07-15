#include "NumberDisplayDataModel.h"

#include "NumericType.h"
NumberDisplayDataModel::
NumberDisplayDataModel()
    : _label(new QLabel())
{
    _label->setMargin(3);
}


unsigned int
NumberDisplayDataModel::
nPorts(PortType portType) const
{
    unsigned int result = 1;

    switch (portType)
    {
    case PortType::In:
        result = 1;
        break;

    case PortType::Out:
        result = 0;

    default:
        break;
    }

    return result;
}


NodeDataType
NumberDisplayDataModel::
dataType(PortType, PortIndex) const
{
    return NumericType<double>().type();
}


std::shared_ptr<NodeData>
NumberDisplayDataModel::
outData(PortIndex)
{
    std::shared_ptr<NodeData> ptr;
    return ptr;
}


void
NumberDisplayDataModel::
setInData(std::shared_ptr<NodeData> data, PortIndex const)
{
    auto numberData = std::dynamic_pointer_cast<NumericType<double>>(data);

    if (numberData)
    {
        NodeValidationState s;
        s._state = NodeValidationState::State::Valid;
        setValidationState(s);
        _label->setText(numberData->numberAsText());
    }
    else
    {
        NodeValidationState s;
        s._state = NodeValidationState::State::Warning;
        s._stateMessage = QStringLiteral("Missing or incorrect inputs");
        setValidationState(s);
        _label->clear();
    }

    _label->adjustSize();
}
