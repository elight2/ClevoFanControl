#include "ExternalFan.h"

#include <algorithm>
#include <numeric>
#include <qcontainerfwd.h>
#include <qdebug.h>
#include <qfile.h>
#include <qlist.h>
#include <qlogging.h>
#include <qthread.h>
#include <qserialport.h>
#include <stdexcept>
#include "../defines.h"

void ExternalFan::initPort(QSerialPort &port,QString name) {
    port.setPortName(name);
    port.setBaudRate(QSerialPort::Baud9600);
    port.setDataBits(QSerialPort::Data8);
    port.setStopBits(QSerialPort::OneStop);
    port.setParity(QSerialPort::NoParity);
    port.setFlowControl(QSerialPort::NoFlowControl);

    port.close();
    if (!port.open(QIODevice::ReadWrite)) {
        cfcUtils::writeLog("ex fan fail to open port: "+name);
        cfcUtils::writeLog("error: "+port.errorString());
        throw std::runtime_error("ex fan fail to open port");
    }
}

ExternalFan::ExternalFan() {
    cfcUtils::writeLog("init ex fan");

    //read cfg
    QFile cfgFile(EX_FAN_CFG_FILE_DIR);
    cfgFile.open(QIODevice::ReadOnly);

    disabled=cfgFile.readLine().trimmed()=="0";
    if (disabled)
        return;

    QString portName1,portName2;
#ifdef __linux__
    portName1=cfgFile.readLine().trimmed();
    portName2=cfgFile.readLine().trimmed();
    cfgFile.readLine();
    cfgFile.readLine();
#elif _WIN32
    cfgFile.readLine();
    cfgFile.readLine();
    portName1=cfgFile.readLine().trimmed();
    portName2=cfgFile.readLine().trimmed();
#endif
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
    cfcUtils::writeLog("port2 name: "+portName1);

    initPort(port1, portName1);
    // initPort(port2, portName2);
    cfcUtils::writeLog("ex fan init finish");
}

ExternalFan::~ExternalFan() {
    if (disabled)
        return;
    port1.close();
    delete [] fanTables[0];
    delete [] fanTables[1];
    // port2.close();
}

void ExternalFan::setSpeed(QSerialPort &port, int num, int speed) {
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
        setSpeed(port1, 1, std::clamp(targetSpeed1,0,100));
        setSpeed(port1, 2, std::clamp(targetSpeed2,0,100));

        lastControlTime=currentTime;
    }
}
