#include "ExternalFan.h"

#include <qcontainerfwd.h>
#include <qdebug.h>
#include <qfile.h>
#include <qimage.h>
#include <qlogging.h>
#include <qserialport.h>
#include <stdexcept>
#include "../utils.h"

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

    port.setPortName(portName);
    port.setBaudRate(QSerialPort::Baud9600);
    port.setDataBits(QSerialPort::Data8);
    port.setParity(QSerialPort::NoParity);
    port.setStopBits(QSerialPort::OneStop);
}

void ExternalFan::adjustFan(int index,int temp,float power) {
    //open
    if (!port.isOpen()) {
        if (!port.open(QIODevice::ReadWrite)) {
            cfcUtils::writeLog("WARNING: ex fan fail to open port: "+portName);
            cfcUtils::writeLog("error: "+port.errorString());
            return;
        }
    }
    port.close();
    port.open(QIODevice::ReadWrite);

    qDebug()<<"hhh";
    port.write(QByteArray("read"));
    qDebug()<<port.read(3);
    port.close();
}
