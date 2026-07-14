#pragma once

#include <QtNodes/NodeDelegateModel>

using QtNodes::NodeDataType;
using QtNodes::NodeData;

class TextData : public NodeData
{
public:
    TextData() : _text("") {}

    TextData(QString const &text) : _text(text) {}

    NodeDataType type() const override
    {
        return NodeDataType {"text", "Text"};
    }

    QString text() const { return _text; }

    void setText(QString const &text) { _text = text; }

private:
    QString _text;
};
