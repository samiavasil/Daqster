#pragma once

#include <QtNodes/DataFlowGraphModel>

class ChatGraphModel : public QtNodes::DataFlowGraphModel
{
public:
    using QtNodes::DataFlowGraphModel::DataFlowGraphModel;
    bool loopsEnabled() const override { return true; }
};
