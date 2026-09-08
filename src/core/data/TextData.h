#pragma once

#include <QtNodes/NodeDelegateModel>

class TextData : public QtNodes::NodeData
{
public:
    TextData() : _text("") {}

    TextData(QString const &text) : _text(text) {}

    QtNodes::NodeDataType type() const override
    {
        return QtNodes::NodeDataType {"text", "Text"};
    }

    QString text() const { return _text; }

    void setText(QString const &text) { _text = text; }

private:
    QString _text;
};
