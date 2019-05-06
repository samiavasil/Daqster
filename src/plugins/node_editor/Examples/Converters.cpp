#include "Converters.h"

#include <QtGui/QDoubleValidator>

#include "DecimalData.h"
#include "IntegerData.h"



std::shared_ptr<NodeData>
DecimalToIntegerConverter::
operator()(std::shared_ptr<NodeData> data)
{
    auto numberData =
            std::dynamic_pointer_cast<DecimalData>(data);

    if (numberData)
    {
        _integer = std::make_shared<IntegerData>(numberData->number());
    }

    return _integer;
}


std::shared_ptr<NodeData>
IntegerToDecimalConverter::
operator()(std::shared_ptr<NodeData> data)
{
    auto numberData =
            std::dynamic_pointer_cast<IntegerData>(data);

    if (numberData)
    {
        _decimal = std::make_shared<DecimalData>(numberData->number());
    }

    return _decimal;
}


std::shared_ptr<QtNodes::NodeData>
DecimalToComplexIntConverter::
operator()(std::shared_ptr<QtNodes::NodeData> data)
{
    auto numberData =
            std::dynamic_pointer_cast<DecimalData>(data);

    if (numberData)
    {
        _complex = std::make_shared<NumericType<int>>(numberData->number());
    }

    return _complex;
}

std::shared_ptr<QtNodes::NodeData>
ComplexIntToDecimalConverter::
operator()(std::shared_ptr<QtNodes::NodeData> data)
{
    auto numberData =
            std::dynamic_pointer_cast<NumericType<int>>(data);

    if (numberData)
    {
        _decimal = std::make_shared<DecimalData>(numberData->number());
    }

    return _decimal;
}


template<typename _T1, typename _T2>
std::shared_ptr<QtNodes::NodeData> AnyToAnyComplexIntConverter<_T1, _T2>::operator()(std::shared_ptr<QtNodes::NodeData> data)
{
    auto numberData =
            std::dynamic_pointer_cast<NumericType<_T1>>(data);

    if (numberData)
    {
        _decimal = std::make_shared<NumericType<_T2>>(numberData->number());
    }

    return _decimal;
}
