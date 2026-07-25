#include "NumberSourceDataUi.h"
#include "ui_NumberSourceDataUi.h"

NumberSourceDataUi::NumberSourceDataUi(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::NumberSourceDataUi)
{
    ui->setupUi(this);
}

NumberSourceDataUi::~NumberSourceDataUi()
{
}

QLineEdit& NumberSourceDataUi::lineEdit(){
    return *(ui->lineEdit);
}

QCheckBox& NumberSourceDataUi::randomEnabled(){
    return *(ui->randomEnabled);
}

QSpinBox& NumberSourceDataUi::intervalSpin(){
    return *(ui->intervalSpin);
}
