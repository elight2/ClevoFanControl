#include "ExternalFan.h"

#include <numeric>
#include <qcontainerfwd.h>
#include <qdebug.h>
#include <qfile.h>
#include <qlogging.h>
#include <qthread.h>
#include <qserialport.h>
#include <stdexcept>
#include "../utils.h"
#include "../defines.h"

const int pwrListLen=10;
const int controlInterval=1000;
const cfcUtils::curvePoint fanTable1[]={
    {0,25},
    {60,25},
    {230,80}
};

void ExternalFan::init() {
    cfcUtils::writeLog("init ex fan");

    //read cfg
    QFile cfgFile(EX_FAN_CFG_FILE_DIR);
    cfgFile.open(QIODevice::ReadOnly);
#ifdef __linux__
    portName=cfgFile.readLine().trimmed();
#elif _WIN32
    cfgFile.readLine();
    portName=cfgFile.readLine().trimmed();
#endif
    cfgFile.close();
    cfcUtils::writeLog("port name: "+portName);

    port1.setPortName(portName);
    port1.setBaudRate(QSerialPort::Baud9600);
    port1.setDataBits(QSerialPort::Data8);
    port1.setStopBits(QSerialPort::OneStop);
    port1.setParity(QSerialPort::NoParity);
    port1.setFlowControl(QSerialPort::NoFlowControl);
}

void ExternalFan::setSpeed(QSerialPort &port, int num, int speed) {
    //open
    if (!port.isOpen()) {
        if (!port.open(QIODevice::ReadWrite)) {
            cfcUtils::writeLog("WARNING: ex fan fail to open port: "+portName);
            cfcUtils::writeLog("error: "+port.errorString());
            return;
        }
    }

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
    port.write(data);
    port.waitForBytesWritten();
    QThread::msleep(cfcDef::MIN_CONTROL_INTERVAL);
    port.close();
}

void ExternalFan::adjustFan(int index,float power) {
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
        int targetSpeed=cfcUtils::calcTable(fanTable1, sizeof(fanTable1)/sizeof(cfcUtils::curvePoint), totalAvg);
        setSpeed(port1, 1, targetSpeed);
        qDebug()<<"fan "<<index<<" pwr "<<cAvg<<"+"<<gAvg<<" % "<<targetSpeed;

        lastControlTime=currentTime;
    }
}
