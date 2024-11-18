#include "CFCmonitor.h"
#include <qobject.h>

CFCmonitor::CFCmonitor(QWidget *parent) : QWidget(parent) {
    this->setAttribute(Qt::WA_QuitOnClose, 0);
    this->setFixedSize(200,300);
    ui.setupUi(this);
}

void CFCmonitor::updateValue(int index, int speed, int rpm, int temperature) {
    //qDebug()<<"CFCmonitor::updateValue ";
    if(index==1)
    {
        ui.label_2->setText(QString::number(temperature));
        if(speed==-2)
            ui.label_6->setText("Auto");
        else
            ui.label_6->setText(QString::number(temperature));
        ui.label_11->setText(QString::number(temperature));
    }
    else if(index==2)
    {
        ui.label_4->setText(QString::number(temperature));
        if(speed==-2)
            ui.label_8->setText("Auto");
        else
            ui.label_8->setText(QString::number(temperature));
        ui.label_12->setText(QString::number(temperature));
    }
    //qDebug()<<"CFCmonitor::updateValue finish";
    return;
}