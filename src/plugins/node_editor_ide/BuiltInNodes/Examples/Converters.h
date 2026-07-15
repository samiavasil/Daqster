#pragma once

#include "DecimalData.h"
#include "IntegerData.h"
#include "NumericType.h"

class DecimalData;
class IntegerData;


class DecimalToIntegerConverter
{

public:

    std::shared_ptr<QtNodes::NodeData>
    operator()(std::shared_ptr<QtNodes::NodeData> data);

private:

    std::shared_ptr<QtNodes::NodeData> _integer;
};


class IntegerToDecimalConverter
{

public:

    std::shared_ptr<QtNodes::NodeData>
    operator()(std::shared_ptr<QtNodes::NodeData> data);

private:

    std::shared_ptr<QtNodes::NodeData> _decimal;
};


class DecimalToComplexIntConverter
{

public:

    std::shared_ptr<QtNodes::NodeData>
    operator()(std::shared_ptr<QtNodes::NodeData> data);

private:

    std::shared_ptr<QtNodes::NodeData> _complex;
};


class ComplexIntToDecimalConverter
{

public:

    std::shared_ptr<QtNodes::NodeData>
    operator()(std::shared_ptr<QtNodes::NodeData> data);

private:

    std::shared_ptr<QtNodes::NodeData> _decimal;
};

template<typename SourceType, typename TargetType>
class AnyToAnyComplexIntConverter
{

public:

    std::shared_ptr<QtNodes::NodeData>
    operator()(std::shared_ptr<QtNodes::NodeData> data);

private:

    std::shared_ptr<QtNodes::NodeData> _decimal;
};


template class AnyToAnyComplexIntConverter<double, int>;
template class AnyToAnyComplexIntConverter<int, double>;
