#ifndef NUMBESOURCEDATAUI_H
#define NUMBESOURCEDATAUI_H

#include <QWidget>

namespace Ui {
class NumbeSourceDataUi;
}
class QLineEdit;
class QSlider;
class NumbeSourceDataUi : public QWidget
{
    Q_OBJECT

public:
    explicit NumbeSourceDataUi(QWidget *parent = 0);
    ~NumbeSourceDataUi();

    QLineEdit &lineEdit();
    QSlider &timeSlider();
private:
    Ui::NumbeSourceDataUi *ui;
};

#endif // NUMBESOURCEDATAUI_H
