#pragma once

#include <QtNodes/NodeDelegateModel>
#include <QImage>

class ImageData : public QtNodes::NodeData
{
public:
    ImageData() {}

    ImageData(QImage const &image) : _image(image) {}

    QtNodes::NodeDataType type() const override
    {
        return QtNodes::NodeDataType {"image", "Image"};
    }

    QImage image() const { return _image; }

    bool isEmpty() const { return _image.isNull(); }

    int width() const { return _image.width(); }

    int height() const { return _image.height(); }

private:
    QImage _image;
};
