#pragma once

#include <QtNodes/NodeDelegateModel>

class FloatData : public QtNodes::NodeData
{
public:
    FloatData() : _value(0.0f) {}

    FloatData(float value) : _value(value) {}

    QtNodes::NodeDataType type() const override
    {
        return QtNodes::NodeDataType {"float", "Float"};
    }

    float value() const { return _value; }

    QString valueAsText() const
    {
        return QString::number(static_cast<double>(_value), 'f', 6);
    }

private:
    float _value;
};
