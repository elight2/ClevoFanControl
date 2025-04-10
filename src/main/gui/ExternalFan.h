#ifndef EXTERNAL_FAN_H
#define EXTERNAL_FAN_H

#include <qlist.h>
#include <qobject.h>
#include <qserialport.h>
#include <qstring.h>
#include <qtmetamacros.h>
#include <vector>
#include <qdatetime.h>

class ExternalFan : public QObject {
Q_OBJECT

public:
    void init();
    void adjustFan(int index,float power);

private:
    QString EX_FAN_CFG_FILE_DIR="./ex_fan.txt";
    QString portName;
    QSerialPort port1;
    qint64 lastControlTime = 0;
    qint64 currentTime = 0;

    std::vector<float> pwrList[2];

    void setSpeed(QSerialPort &port, int num, int speed);

signals:
    void adjustFanSig(int index,float power);
};

#endif
