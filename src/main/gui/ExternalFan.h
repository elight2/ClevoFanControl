#ifndef EXTERNAL_FAN_H
#define EXTERNAL_FAN_H

#include <qobject.h>
#include <qserialport.h>
#include <qstring.h>
#include <qtmetamacros.h>

class ExternalFan : public QObject {
Q_OBJECT

public:
    void init();
    void adjustFan(int index,int temp,float power);

private:
    QString EX_FAN_CFG_FILE_DIR="./ex_fan.txt";
    QString portName;
    QSerialPort port;

signals:
    void adjustFanSig(int index,int temp,float power);
};

#endif
