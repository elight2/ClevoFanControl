#include "CFCconfig.h"

CFCconfig::CFCconfig(QWidget *parent, ConfigManager *cfg) : QWidget(parent) {
    config=cfg;

    this->setAttribute(Qt::WA_QuitOnClose, 0);
    ui.setupUi(this);

    //profiles
    for(int i=0;i<config->profileCount;i++)
        ui.comboBox->addItem(config->fanProfiles[i].name);

    setCurProfileOptions();
    setOptions();

    QObject::connect(ui.pushButton, &QPushButton::clicked, this, &CFCconfig::ok);
    QObject::connect(ui.pushButton_2, &QPushButton::clicked, this, &CFCconfig::apply);
    QObject::connect(ui.comboBox, &QComboBox::currentTextChanged, this, &CFCconfig::setCurProfileOptions);
}

void CFCconfig::setOptions() {
    ui.checkBox->setChecked(config->useStaticSpeed);
    ui.lineEdit_35->setText(QString::number(config->staticSpeed[0]));
    ui.lineEdit_37->setText(QString::number(config->staticSpeed[1]));
    ui.checkBox_2->setChecked(config->useSpeedLimit);
    ui.lineEdit_36->setText(QString::number(config->speedLimit[0]));
    ui.lineEdit_38->setText(QString::number(config->speedLimit[1]));
    ui.lineEdit_6->setText(QString::number(config->timeIntervals[0]));
    ui.lineEdit_7->setText(QString::number(config->timeIntervals[1]));
    ui.checkBox_3->setChecked(config->useClevoAuto);
    ui.checkBox_4->setChecked(config->maxSpeed);
    ui.checkBox_5->setChecked(config->monitorGpu);
}

void CFCconfig::setCurProfileOptions() {
    qDebug()<<"setCurProfileOptions";
    int curProfileIndex=getCurProfileIndex();

    //cpu
    ui.radioButton->setChecked(config->fanProfiles[curProfileIndex].inUse[0] == 2);//ts check
    ui.radioButton_2->setChecked(config->fanProfiles[curProfileIndex].inUse[0] == 1);//mt check
    for (int i = 0; i < 10; i++)
        ui.tableWidget->removeRow(0);
    for (int i = 0; i < 10; i++) {
        ui.tableWidget->insertRow(i);
        ui.tableWidget->setItem(i, 0, new QTableWidgetItem(
                QString::number(config->fanProfiles[curProfileIndex].TStempList[0][i])));//temp list
        ui.tableWidget->setItem(i, 1, new QTableWidgetItem(
                QString::number(config->fanProfiles[curProfileIndex].TSspeedList[0][i])));//speed list
    }
    ui.lineEdit->setText(QString::number(config->fanProfiles[curProfileIndex].MTconfig[0][0]));//mt max temp
    ui.lineEdit_2->setText(QString::number(config->fanProfiles[curProfileIndex].MTconfig[0][1]));//mt min temp
    ui.lineEdit_4->setText(QString::number(config->fanProfiles[curProfileIndex].MTconfig[0][2]));//mt min speed
    ui.lineEdit_5->setText(QString::number(config->fanProfiles[curProfileIndex].MTconfig[0][3]));//mt step

    //gpu 2
    ui.radioButton_11->setChecked(config->fanProfiles[curProfileIndex].inUse[1] == 2);
    ui.radioButton_12->setChecked(config->fanProfiles[curProfileIndex].inUse[1] == 1);
    for (int i = 0; i < 10; i++)
        ui.tableWidget_6->removeRow(0);
    for (int i = 0; i < 10; i++) {
        ui.tableWidget_6->insertRow(i);
        ui.tableWidget_6->setItem(i, 0, new QTableWidgetItem(
                QString::number(config->fanProfiles[curProfileIndex].TStempList[1][i])));
        ui.tableWidget_6->setItem(i, 1, new QTableWidgetItem(
                QString::number(config->fanProfiles[curProfileIndex].TSspeedList[1][i])));
    }
    ui.lineEdit_26->setText(QString::number(config->fanProfiles[curProfileIndex].MTconfig[1][0]));
    ui.lineEdit_27->setText(QString::number(config->fanProfiles[curProfileIndex].MTconfig[1][1]));
    ui.lineEdit_29->setText(QString::number(config->fanProfiles[curProfileIndex].MTconfig[1][2]));
    ui.lineEdit_0->setText(QString::number(config->fanProfiles[curProfileIndex].MTconfig[1][3]));
    qDebug()<<"setCurProfileOptions finish";
}

int CFCconfig::getCurProfileIndex() {
    QString curProfileName=ui.comboBox->currentText();
    int curProfileIndex=0;
    for(int i=0;i<config->profileCount;i++) {
        if(config->fanProfiles[i].name==curProfileName) {
            curProfileIndex=i;
            break;
        }
    }
    return curProfileIndex;
}

void CFCconfig::syncOptions() {
    int curProfileIndex=getCurProfileIndex();
    
    for (int i = 0; i < 10; i++) {
        config->fanProfiles[curProfileIndex].TStempList[0][i]=ui.tableWidget->item(i, 0)->text().toInt();
        config->fanProfiles[curProfileIndex].TSspeedList[0][i]=ui.tableWidget->item(i, 1)->text().toInt();
    }
    for (int i = 0; i < 10; i++) {
        config->fanProfiles[curProfileIndex].TStempList[1][i]=ui.tableWidget_6->item(i, 0)->text().toInt();
        config->fanProfiles[curProfileIndex].TSspeedList[1][i]=ui.tableWidget_6->item(i, 1)->text().toInt();
    }
    
    config->fanProfiles[curProfileIndex].inUse[0]=ui.radioButton->isChecked() ? 2 : 1;
    config->fanProfiles[curProfileIndex].MTconfig[0][0]=ui.lineEdit->text().toInt();
    config->fanProfiles[curProfileIndex].MTconfig[0][1]=ui.lineEdit_2->text().toInt();
    config->fanProfiles[curProfileIndex].MTconfig[0][2]=ui.lineEdit_4->text().toInt();
    config->fanProfiles[curProfileIndex].MTconfig[0][3]=ui.lineEdit_5->text().toInt();

    config->fanProfiles[curProfileIndex].inUse[1]=ui.radioButton_11->isChecked() ? 2 : 1;
    config->fanProfiles[curProfileIndex].MTconfig[1][0]=ui.lineEdit_26->text().toInt();
    config->fanProfiles[curProfileIndex].MTconfig[1][1]=ui.lineEdit_27->text().toInt();
    config->fanProfiles[curProfileIndex].MTconfig[1][2]=ui.lineEdit_29->text().toInt();
    config->fanProfiles[curProfileIndex].MTconfig[1][3]=ui.lineEdit_0->text().toInt();

    config->useStaticSpeed=ui.checkBox->isChecked();
    config->staticSpeed[0]=ui.lineEdit_35->text().toInt();
    config->staticSpeed[1]=ui.lineEdit_37->text().toInt();
    config->useSpeedLimit=ui.checkBox_2->isChecked();
    config->speedLimit[0]=ui.lineEdit_36->text().toInt();
    config->speedLimit[1]=ui.lineEdit_38->text().toInt();
    config->timeIntervals[0]=ui.lineEdit_6->text().toInt();
    config->timeIntervals[1]=ui.lineEdit_7->text().toInt();
    config->useClevoAuto=ui.checkBox_3->isChecked();
    config->maxSpeed=ui.checkBox_4->isChecked();
    config->monitorGpu=ui.checkBox_5->isChecked();
}

void CFCconfig::ok() {
    qDebug()<<"clicked OK";
    apply();
    this->hide();
}

void CFCconfig::apply() {
    qDebug()<<"clicked apply";
    syncOptions();
    config->saveToJson();
    emit cfgWindowUpdate();
}
