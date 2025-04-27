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
#include <string>
#include "../defines.h"
#include "nlohmann/json.hpp"

const QString ExternalFan::EX_FAN_CFG_FILE_DIR="./ex_fan.json";
const float ExternalFan::portInfo[2]={50.0,50.1};
const QString ExternalFan::exFanLogFlag="EX_FAN: ";
#ifdef __linux__
const QString ExternalFan::ch341Describe="USB Serial";
#elif _WIN32
const QString ExternalFan::ch341Describe="USB-SERIAL CH340";
#endif

//where the fan is
ExFanInfo ExternalFan::fanInfoList[4]={
    {0,1},
    {0,2},
    {0,3},
    {1,3}
};

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
        cfcUtils::writeLog(exFanLogFlag+"initPort: fail to open port: "+name+", err: "+port.errorString());
    }
}

QString ExternalFan::searchPort(int index) {
    auto allPorts=QSerialPortInfo::availablePorts();
    for (auto i : allPorts) {
        if (i.description()==ch341Describe) { //only USB ones
            QSerialPort curPort;
            initPort(curPort, i.portName());
            if (!curPort.isOpen())
                return "NOT_FOUND";
            curPort.write("read");
            curPort.waitForBytesWritten();
            curPort.waitForReadyRead();
            QString content=curPort.readAll();
            curPort.close();
            
            if (content.mid(1,4)==QString::number(portInfo[index],'f',1))
                return i.portName();
        }
    }

    return "NOT_FOUND";
}

ExternalFan::ExternalFan() {
    cfcUtils::writeLog(exFanLogFlag+"init ex fan");

    //read cfg
    QFile cfgFile(EX_FAN_CFG_FILE_DIR);
    cfgFile.open(QIODevice::ReadOnly);
    nlohmann::json cfgJson=nlohmann::json::parse(cfgFile.readAll().toStdString());
    cfgFile.close();

    enabled=cfgJson["enabled"];
    if (!enabled)
        return;

    //search port
    for (int i=0;i<sizeof(ports)/sizeof(QSerialPort);i++) {
        QString name=searchPort(i);
        cfcUtils::writeLog(exFanLogFlag+"port"+QString::number(i)+": "+name);
        initPort(ports[i],name);
    }

    //read fan tables
    pwrListLen=cfgJson["pwrListLen"];
    controlInterval=cfgJson["controlInterval"];
    for (int i=0;i<sizeof(fanInfoList)/sizeof(ExFanInfo);i++) {
        fanInfoList[i].pwrType=cfgJson["fans"][std::to_string(i)]["pwrType"];
        nlohmann::json::array_t pwrList=cfgJson["fans"][std::to_string(i)]["pwrList"];
        nlohmann::json::array_t speedList=cfgJson["fans"][std::to_string(i)]["speedList"];
        fanInfoList[i].tableLen=pwrList.size();
        fanInfoList[i].table=new cfcUtils::curvePoint[fanInfoList[i].tableLen];
        for (int j=0;j<fanInfoList[i].tableLen;j++)
            fanInfoList[i].table[j]={pwrList[j],speedList[j]};

        // QByteArrayList table=cfgFile.readLine().trimmed().split(' ');
        // QByteArrayList tablev=cfgFile.readLine().trimmed().split(' ');
        // fanTableLens[i]=table.size();
        // qDebug()<<tablev.size();
        // fanTables[i]=new cfcUtils::curvePoint[fanTableLens[i]];
        // for (int j=0;j<fanTableLens[i];j++)
        //     fanTables[i][j]={table[j].toInt(),tablev[j].toInt()};
    }

    cfcUtils::writeLog(exFanLogFlag+"ex fan init finish");
}

ExternalFan::~ExternalFan() {
    if (!enabled)
        return;
    ports[0].close();
    ports[1].close();
    // for (int i=0;i<sizeof(fanTables)/sizeof(cfcUtils::curvePoint*);i++)
    //     delete [] fanTables[i];

    for (int i=0;i<sizeof(fanInfoList)/sizeof(ExFanInfo);i++)
        delete [] fanInfoList[i].table;
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
    if (ports[index].error()==QSerialPort::NoError) { // normal
        if (!ports[index].isOpen()) { // open
            cfcUtils::writeLog(exFanLogFlag+"port "+QString::number(index)+" no error but not open, reopening");
            ports[index].open(QIODevice::ReadWrite);
        } else { // write
            ports[index].write(data);
            ports[index].waitForBytesWritten();
        }
    } else { //fix error
        cfcUtils::writeLog(exFanLogFlag+"port "+ports[index].portName()+" error: "+QString::number(ports[index].error())+", fixing");

        //first close all ports
        for (int i=0;i<sizeof(ports)/sizeof(QSerialPort);i++)
            ports[i].close();
        
        //then research all ports
        for (int i=0;i<sizeof(ports)/sizeof(QSerialPort);i++) {
            QString portName=searchPort(i);
            if (portName=="NOT_FOUND") {
                cfcUtils::writeLog(exFanLogFlag+"port "+QString::number(i)+" not found when fixing error");
            } else {
                ports[i].close();
                ports[i].setPortName(portName);
                ports[i].open(QIODevice::ReadWrite);
                cfcUtils::writeLog(exFanLogFlag+"port "+QString::number(i)+" fixed");
            }
        }
    }
    // QThread::msleep(cfcDef::MIN_CONTROL_INTERVAL);
}

void ExternalFan::adjustFan(int index,float power) {
    if (!enabled)
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
        qDebug()<<"pwr: "<<cAvg<<"+"<<gAvg<<"="<<cAvg+gAvg;

        for (int i=0;i<sizeof(fanInfoList)/sizeof(ExFanInfo);i++) {
            int value=0;
            switch (fanInfoList[i].pwrType) {
                case 0:
                    value=cAvg+gAvg;
                    break;
                case 1:
                    value=cAvg;
                    break;
                case 2:
                    value=gAvg;
                    break;
            }
            int targetSpeed=cfcUtils::calcTable(fanInfoList[i].table, fanInfoList[i].tableLen, value);
            setSpeed(fanInfoList[i].port, fanInfoList[i].num, std::clamp(targetSpeed,0,100));
            qDebug()<<"fan"<<i<<": "<<targetSpeed;
        }

        // int targetSpeed1=cfcUtils::calcTable(fanTables[0], fanTableLens[0], totalAvg);
        // int targetSpeed2=cfcUtils::calcTable(fanTables[1], fanTableLens[1], totalAvg);
        // int targetSpeed3=cfcUtils::calcTable(fanTables[2], fanTableLens[2], totalAvg);
        // int targetSpeed4=cfcUtils::calcTable(fanTables[3], fanTableLens[3], cAvg);
        // setSpeed(0, 1, std::clamp(targetSpeed1,0,100));
        // setSpeed(0, 2, std::clamp(targetSpeed2,0,100));
        // setSpeed(0, 3, std::clamp(targetSpeed3,0,100));
        // setSpeed(1, 3, std::clamp(targetSpeed4,0,100));
        // qDebug()<<cAvg<<"+"<<gAvg<<"="<<totalAvg<<"#1:"<<targetSpeed1<<"#2:"<<targetSpeed2<<"#3"<<targetSpeed3<<"#4"<<targetSpeed4;

        lastControlTime=currentTime;
    }
}
