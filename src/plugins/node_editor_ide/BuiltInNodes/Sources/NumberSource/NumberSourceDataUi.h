#ifndef NUMBESOURCEDATAUI_H
#define NUMBESOURCEDATAUI_H

#include <QWidget>

namespace Ui {
class NumberSourceDataUi;
}
class QLineEdit;
class QCheckBox;
class QSpinBox;
class NumberSourceDataUi : public QWidget
{
    Q_OBJECT

public:
    explicit NumberSourceDataUi(QWidget *parent = nullptr);
    ~NumberSourceDataUi();

    QLineEdit &lineEdit();
    QCheckBox &randomEnabled();
    QSpinBox &intervalSpin();
private:
    Ui::NumberSourceDataUi *ui;
};

#endif // NUMBESOURCEDATAUI_H
