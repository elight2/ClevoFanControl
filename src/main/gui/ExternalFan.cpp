#include "ExternalFan.h"

#include <algorithm>
#include <exception>
#include <numeric>
#include <qcontainerfwd.h>
#include <qcoreapplication.h>
#include <qdebug.h>
#include <qfile.h>
#include <qlist.h>
#include <qlogging.h>
#include <qobject.h>
#include <qthread.h>
#include <qserialport.h>
#include <qserialportinfo.h>
#include <qtmetamacros.h>
#include <stdexcept>
#include <string>
#include "../defines.h"
#include "nlohmann/json.hpp"

const QString ExternalFan::EX_FAN_CFG_FILE_DIR="./data/ex_fan.json";
const float ExternalFan::portInfo[2]={50.0,50.1};
const QString ExternalFan::exFanLogFlag="EX_FAN: ";
const int ExternalFan::minControlInterval=100;
const int ExternalFan::portCount=2;
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
        emit CfcLogMgr::LOG_MGR->writeLog(exFanLogFlag+"initPort: fail to open port: "+name+", err: "+port.errorString());
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
    
}

ExternalFan::~ExternalFan() {
    if (!enabled)
        return;
    for (int i=0;i<portCount;i++)
        ports[i].close();
    delete [] ports;

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
            emit CfcLogMgr::LOG_MGR->writeLog(exFanLogFlag+"port "+QString::number(index)+" no error but not open, reopening");
            ports[index].open(QIODevice::ReadWrite);
        } else { // write
            ports[index].clear();
            ports[index].write(data);
            ports[index].waitForBytesWritten();
            ports[index].waitForReadyRead();
            QString res=ports[index].readAll();
            ports[index].clear();
            if (res=="FAIL\n")
                emit CfcLogMgr::LOG_MGR->writeLog(exFanLogFlag+"warn: port "+ports[index].portName()+" num "+QString::number(num)+" failed to apply speed");
        }
    } else { //fix error
        emit CfcLogMgr::LOG_MGR->writeLog(exFanLogFlag+"port "+ports[index].portName()+" error: "+QString::number(ports[index].error())+", fixing");

        //first close all ports
        for (int i=0;i<portCount;i++)
            ports[i].close();
        
        //then research all ports
        for (int i=0;i<portCount;i++) {
            QString portName=searchPort(i);
            if (portName=="NOT_FOUND") {
                emit CfcLogMgr::LOG_MGR->writeLog(exFanLogFlag+"port "+QString::number(i)+" not found when fixing error");
            } else {
                ports[i].close();
                ports[i].setPortName(portName);
                ports[i].open(QIODevice::ReadWrite);
                emit CfcLogMgr::LOG_MGR->writeLog(exFanLogFlag+"port "+QString::number(i)+" fixed");
            }
        }
    }
}

void ExternalFan::recordData(int index,float power,bool useMaxSpeed) {
    if (!enabled)
        return;

    this->maxSpeed=useMaxSpeed;

    //record
    pwrList[index-1].push_back(power);
    if (pwrList[index-1].size()>pwrListLen)
        pwrList[index-1].erase(pwrList[index-1].begin());
}

void ExternalFan::run() {
    //init
    emit CfcLogMgr::LOG_MGR->writeLog(exFanLogFlag+"init ex fan");

    //read cfg
    QFile cfgFile(EX_FAN_CFG_FILE_DIR);
    cfgFile.open(QIODevice::ReadOnly);
    nlohmann::json cfgJson;
    if (cfgFile.isOpen()) {
        cfgJson=nlohmann::json::parse(cfgFile.readAll().toStdString());
        cfgFile.close();
        enabled=cfgJson["enabled"];
    } else
        enabled=false;

    if (!enabled)
        return;

    //search port
    ports=new QSerialPort[portCount];
    for (int i=0;i<portCount;i++) {
        QString name=searchPort(i);
        emit CfcLogMgr::LOG_MGR->writeLog(exFanLogFlag+"port"+QString::number(i)+": "+name);
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
        fanInfoList[i].table=new CfcUtils::curvePoint[fanInfoList[i].tableLen];
        for (int j=0;j<fanInfoList[i].tableLen;j++)
            fanInfoList[i].table[j]={pwrList[j],speedList[j]};
    }

    emit CfcLogMgr::LOG_MGR->writeLog(exFanLogFlag+"ex fan init finish");

    while (!isInterruptionRequested()) {
        currentTime=QDateTime::currentMSecsSinceEpoch();
        //control
        if (currentTime>lastControlTime+controlInterval) {
            float cAvg=std::accumulate(pwrList[0].begin(),pwrList[0].end(),0.0)/pwrList[0].size();
            float gAvg=std::accumulate(pwrList[1].begin(),pwrList[1].end(),0.0)/pwrList[1].size();
            qDebug()<<"pwr: "<<cAvg<<"+"<<gAvg<<"="<<cAvg+gAvg;

            for (int i=0;i<sizeof(fanInfoList)/sizeof(ExFanInfo);i++) {
                int value=0;
                int targetSpeed=0;
                if (maxSpeed)
                    targetSpeed=100;
                else {
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
                    if (value<0)
                        value=0;
                    targetSpeed=CfcUtils::calcTable(fanInfoList[i].table, fanInfoList[i].tableLen, value);
                    targetSpeed=std::clamp(targetSpeed,0,100);
                }
                setSpeed(fanInfoList[i].port, fanInfoList[i].num, targetSpeed);
                qDebug()<<"fan"<<i<<": "<<targetSpeed;
                QThread::msleep(80);
            }

            lastControlTime=currentTime;
        }
        QThread::msleep(ExternalFan::minControlInterval);
    }
}
