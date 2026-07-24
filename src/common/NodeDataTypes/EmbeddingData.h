#pragma once

#include <QtNodes/NodeDelegateModel>
#include <vector>

class EmbeddingData : public QtNodes::NodeData
{
public:
    EmbeddingData() : _dim(0) {}

    EmbeddingData(std::vector<float> const &values)
        : _values(values), _dim(values.size()) {}

    QtNodes::NodeDataType type() const override
    {
        return QtNodes::NodeDataType {"embedding", "Embedding"};
    }

    std::vector<float> const &values() const { return _values; }

    size_t dim() const { return _dim; }

    bool isEmpty() const { return _values.empty(); }

    float const *data() const { return _values.data(); }

private:
    std::vector<float> _values;
    size_t _dim;
};
