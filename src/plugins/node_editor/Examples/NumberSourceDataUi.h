#ifndef NUMBESOURCEDATAUI_H
#define NUMBESOURCEDATAUI_H

#include <QWidget>

namespace Ui {
class NumberSourceDataUi;
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
    Ui::NumberSourceDataUi *ui;
};

#endif // NUMBESOURCEDATAUI_H
