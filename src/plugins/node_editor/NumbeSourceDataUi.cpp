#include "NumberSourceDataUi.h"
#include "ui_NumbeSourceDataUi.h"

NumberSourceDataUi::NumberSourceDataUi(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::NumbeSourceDataUi)
{
    ui->setupUi(this);

}

NumberSourceDataUi::~NumberSourceDataUi()
{
    delete ui;
}

QLineEdit& NumberSourceDataUi::lineEdit(){
    return *(ui->lineEdit);
}

QSlider& NumberSourceDataUi::timeSlider(){
    return *(ui->timeSlider);
}
