#include "ExternalFan.h"

#include <algorithm>
#include <exception>
#include <numeric>
#include <qcontainerfwd.h>
#include <qdebug.h>
#include <qfile.h>
#include <qlist.h>
#include <qlogging.h>
#include <qobject.h>
#include <qthread.h>
#include <qserialport.h>
#include <qserialportinfo.h>
#include <stdexcept>
#include "../defines.h"

void ExternalFan::initPort(int index,QString name) {
    initPort(ports[index],name);
}

void ExternalFan::initPort(QSerialPort &port,QString name) {
    port.setPortName(name);
    port.setBaudRate(QSerialPort::Baud9600);
    port.setDataBits(QSerialPort::Data8);
    port.setStopBits(QSerialPort::OneStop);
    port.setParity(QSerialPort::NoParity);
    port.setFlowControl(QSerialPort::NoFlowControl);

    port.close();
    if (!port.open(QIODevice::ReadWrite)) {
        cfcUtils::writeLog("ex fan: fail to open port: "+name);
        cfcUtils::writeLog("error: "+port.errorString());
    }
}

QString ExternalFan::searchPort(int index) {
    auto allPorts=QSerialPortInfo::availablePorts();
    for (auto i : allPorts) {
        if (i.description()=="USB Serial") { //only USB ones
            QSerialPort curPort;
            initPort(curPort, i.portName());
            if (!curPort.isOpen())
                return "NOT_FOUND";
            curPort.write("read");
            curPort.waitForReadyRead();
            QString content=curPort.readAll();
            curPort.close();
            if (index==0) {
                if (content[4]=='0') //50.0
                    return i.portName();
            } else if (index==1) {
                if (content[4]=='1') // F50.1
                    return i.portName();
            }
        }
    }

    return "NOT_FOUND";
}

ExternalFan::ExternalFan() {
    cfcUtils::writeLog("init ex fan");

    //read cfg
    QFile cfgFile(EX_FAN_CFG_FILE_DIR);
    cfgFile.open(QIODevice::ReadOnly);

    disabled=cfgFile.readLine().trimmed()=="0";
    if (disabled) {
        cfgFile.close();
        return;
    }

    //search port
    QString portName1=searchPort(0);
    QString portName2=searchPort(1);

    //read fan tables
    pwrListLen=cfgFile.readLine().trimmed().toInt();
    controlInterval=cfgFile.readLine().trimmed().toInt();
    for (int i=0;i<sizeof(fanTables)/sizeof(cfcUtils::curvePoint*);i++) {
        QByteArrayList table=cfgFile.readLine().trimmed().split(' ');
        QByteArrayList tablev=cfgFile.readLine().trimmed().split(' ');
        fanTableLens[i]=table.size();
        qDebug()<<tablev.size();
        fanTables[i]=new cfcUtils::curvePoint[fanTableLens[i]];
        for (int j=0;j<fanTableLens[i];j++)
            fanTables[i][j]={table[j].toInt(),tablev[j].toInt()};
    }

    cfgFile.close();
    cfcUtils::writeLog("port1 name: "+portName1);
    cfcUtils::writeLog("port2 name: "+portName2);

    initPort(ports[0], portName1);
    initPort(ports[1], portName2);

    cfcUtils::writeLog("ex fan init finish");
}

ExternalFan::~ExternalFan() {
    if (disabled)
        return;
    ports[0].close();
    ports[1].close();
    for (int i=0;i<sizeof(fanTables)/sizeof(cfcUtils::curvePoint*);i++)
        delete [] fanTables[i];
}

void ExternalFan::setSpeed(int index, int num, int speed) {
    //gen str
    char data[7]="D0:000";
    data[1]=num+48;
    data[3]=(speed==100)+48;
    int last=speed%10;
    speed/=10;
    int second=speed%10;
    data[4]=second+48;
    data[5]=last+48;
    
    //write
    if (ports[index].error()==QSerialPort::NoError) {
        if (!ports[index].isOpen()) {
            cfcUtils::writeLog("ex fan: port "+QString::number(index)+" not open, reopening");
            ports[index].open(QIODevice::ReadWrite);
        } else {
            ports[index].write(data);
            ports[index].waitForBytesWritten();
        }
    } else {
        cfcUtils::writeLog("ex fan: port "+ports[index].portName()+" error: "+QString::number(ports[index].error())+", researching");
        QString portName=searchPort(index);
        if (portName=="NOT_FOUND") {
            cfcUtils::writeLog("ex fan: port "+QString::number(index)+" not found");
        } else {
            ports[index].close();
            ports[index].setPortName(portName);
            ports[index].open(QIODevice::ReadWrite);
        }
    }
    QThread::msleep(cfcDef::MIN_CONTROL_INTERVAL);
}

void ExternalFan::adjustFan(int index,float power) {
    if (disabled)
        return;

    currentTime=QDateTime::currentMSecsSinceEpoch();

    //record
    pwrList[index-1].push_back(power);
    if (pwrList[index-1].size()>pwrListLen)
        pwrList[index-1].erase(pwrList[index-1].begin());

    //control
    if (currentTime>lastControlTime+controlInterval) {
        float cAvg=std::accumulate(pwrList[0].begin(),pwrList[0].end(),0.0)/pwrList[0].size();
        float gAvg=std::accumulate(pwrList[1].begin(),pwrList[1].end(),0.0)/pwrList[1].size();
        float totalAvg=cAvg+gAvg;

        int targetSpeed1=cfcUtils::calcTable(fanTables[0], fanTableLens[0], totalAvg);
        int targetSpeed2=cfcUtils::calcTable(fanTables[1], fanTableLens[1], totalAvg);
        int targetSpeed3=cfcUtils::calcTable(fanTables[2], fanTableLens[2], totalAvg);
        setSpeed(0, 1, std::clamp(targetSpeed1,0,100));
        setSpeed(0, 2, std::clamp(targetSpeed2,0,100));
        setSpeed(1, 3, std::clamp(targetSpeed3,0,100));
        qDebug()<<cAvg<<"+"<<gAvg<<"="<<totalAvg<<"#1:"<<targetSpeed1<<"#2:"<<targetSpeed2<<"#3"<<targetSpeed3;

        lastControlTime=currentTime;
    }
}
