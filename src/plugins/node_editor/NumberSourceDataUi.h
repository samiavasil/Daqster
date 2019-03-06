#ifndef NUMBESOURCEDATAUI_H
#define NUMBESOURCEDATAUI_H

#include <QWidget>

namespace Ui {
class NumbeSourceDataUi;
}
class QLineEdit;
class QSlider;
class NumberSourceDataUi : public QWidget
{
    Q_OBJECT

public:
    explicit NumberSourceDataUi(QWidget *parent = 0);
    ~NumberSourceDataUi();

    QLineEdit &lineEdit();
    QSlider &timeSlider();
private:
    Ui::NumbeSourceDataUi *ui;
};

#endif // NUMBESOURCEDATAUI_H
