#include "AudioSourceWidget.h"
#include "ui_AudioSourceWidget.h"

AudioSourceWidget::AudioSourceWidget(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::AudioSourceWidget)
{
    ui->setupUi(this);
}

AudioSourceWidget::~AudioSourceWidget()
{
    delete ui;
}
