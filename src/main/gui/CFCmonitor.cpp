#include "CFCmonitor.h"

CFCmonitor::CFCmonitor(QWidget *parent) : QWidget(parent) {
    this->setAttribute(Qt::WA_QuitOnClose, 0);
    this->setFixedSize(300,200);
    ui.setupUi(this);
}

void CFCmonitor::updateValue(int index, int speed, int rpm, int temperature, double power) {
    //qDebug()<<"CFCmonitor::updateValue ";
    if(index==1) {
        ui.label_2->setText(QString::number(temperature));
        ui.label_4->setText(QString::number(power,'f',2));
        ui.label_6->setText(QString::number(speed));
        ui.label_11->setText(QString::number(rpm));
    }
    else if(index==2) {
        ui.label_15->setText(QString::number(temperature));
        ui.label_16->setText(QString::number(power,'f',2));
        ui.label_8->setText(QString::number(speed));
        ui.label_12->setText(QString::number(rpm));
    }
    //qDebug()<<"CFCmonitor::updateValue finish";
    return;
}