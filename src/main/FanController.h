#ifndef FAN_CONTROLLER_H
#define FAN_CONTROLLER_H

#include <QtCore/qthread.h>
#include <qobject.h>
#include <mutex>
#include <qtmetamacros.h>
#include <QtCore/qprocess.h>

#ifdef _WIN32
#include <tchar.h>
#include <Windows.h>
#include "WinRing0/OlsApiInit.h"
#elif __linux__
#include <sys/io.h>
#include <unistd.h>
#endif

#include "ConfigManager.h"

class FanController : public QThread {
Q_OBJECT

public:
    FanController(std::atomic_bool *shouldRunFlag, std::atomic_bool*isRunningFlag, ConfigManager *configMgr, QObject *parent);
    ~FanController();

protected:
    virtual int getTemp()=0;
    virtual int getRpm()=0;
    void setFanSpeed(int value); //-1 means auto

    void ioOutByte(unsigned short port, unsigned char value);
    unsigned char ioInByte(unsigned short port);
    void waitEc(unsigned short port, unsigned char index, unsigned char value);
    unsigned char readEc_1(unsigned char addr);
    void doEc(unsigned char cmd, unsigned char addr, unsigned char value);

    ConfigManager *config;
    int index;
    static std::mutex EClock;
    std::atomic_bool havePrivilege;
#ifdef _WIN32
    static HMODULE WinRing0m;
#endif
    static std::atomic_bool ioInitialized;

private:
    void run();

#ifdef _WIN32
    bool InitOpenLibSys_m();
    bool DeinitOpenLibSys_m();
#endif

    std::atomic_bool *shouldRun;
    std::atomic_bool *isRunning;
    qint64 lastControlTime = 0;
    qint64 currentTime = 0;
    int curSpeed=30;
    const int minSafeSpeed=10;
    bool isAuto=0;
    int temperature=0;
    int rpm=0;

signals:
    void updateMonitor(int index, int speed, int rpm, int temperature);
};

class CpuFanController : public FanController {
Q_OBJECT

public:
    CpuFanController(std::atomic_bool *shouldRunFlag, std::atomic_bool*isRunningFlag, ConfigManager *configMgr, QObject *parent);

protected:
    int getTemp();
    int getRpm();
};

class GpuFanController : public FanController {
Q_OBJECT

public:
    GpuFanController(std::atomic_bool *shouldRunFlag, std::atomic_bool*isRunningFlag, ConfigManager *configMgr, QObject *parent);

protected:
    int getTemp();
    int getRpm();
};

#endif
