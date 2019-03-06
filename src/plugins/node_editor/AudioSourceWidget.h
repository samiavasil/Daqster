#ifndef AUDIOSOURCEWIDGET_H
#define AUDIOSOURCEWIDGET_H

#include <QWidget>

namespace Ui {
class AudioSourceWidget;
}

class AudioSourceWidget : public QWidget
{
    Q_OBJECT

public:
    explicit AudioSourceWidget(QWidget *parent = 0);
    ~AudioSourceWidget();

private:
    Ui::AudioSourceWidget *ui;
};

#endif // AUDIOSOURCEWIDGET_H
