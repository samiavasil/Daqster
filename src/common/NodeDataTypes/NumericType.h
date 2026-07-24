#pragma once

#include <QtNodes/NodeDelegateModel>

template<typename ValueType>
class NumericType : public QtNodes::NodeData
{
public:
    NumericType() : _number(ValueType{}) {}

    NumericType(ValueType const number) : _number(number) {}

    QtNodes::NodeDataType type() const override
    {
        return QtNodes::NodeDataType {typeid(ValueType).name(),
                                     typeid(ValueType).name()};
    }

    ValueType number() const { return _number; }

    QString numberAsText() const
    {
        if constexpr (std::is_same_v<ValueType, int>)
            return QString::number(_number);
        else
            return QString::number(static_cast<double>(_number), 'f', 6);
    }

private:
    ValueType _number;
};
