#include "NumbeSourceDataUi.h"
#include "ui_NumbeSourceDataUi.h"

NumbeSourceDataUi::NumbeSourceDataUi(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::NumbeSourceDataUi)
{
    ui->setupUi(this);

}

NumbeSourceDataUi::~NumbeSourceDataUi()
{
    delete ui;
}

QLineEdit& NumbeSourceDataUi::lineEdit(){
    return *(ui->lineEdit);
}

QSlider& NumbeSourceDataUi::timeSlider(){
    return *(ui->timeSlider);
}
