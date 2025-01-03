#ifndef FAN_CONTROLLER_H
#define FAN_CONTROLLER_H

#include <chrono>

#include "ConfigManager.h"
#include "../ClevoEcAccessor.h"

#include <QtCore/qthread.h>

class CpuPowerMonitor {
public:
    CpuPowerMonitor(int index);
    double getPower();

private:
    double getCurEnergy();
    void rdmsr(int pos, int len, char *dest);

    double lastEnergy;
    long lastQueryTime;
    int cpuIndex;
    char cpuMsrDir[1024];
    const int MSR_RAPL_POWER_UNIT=0x606;
    const int MSR_PKG_ENERGY_STATUS=0x611;
};

class FanController : public QThread {
Q_OBJECT

public:
    FanController(ConfigManager *config, QObject *parent);
    ~FanController();

    void setShouldStop();

protected:
    virtual int getTemp()=0;
    virtual double getPower()=0;

    ConfigManager *config;
    int index=-1;

private:
    void run();
    int getRpm();

    ClevoEcAccessor accessor;
    std::atomic_bool shouldRun=1;
    std::atomic_bool isRunning=0;
    qint64 lastControlTime = 0;
    qint64 currentTime = 0;
    int curSpeed=30;
    const int minSafeSpeed=10;
    const int minControlInterval=100;
    bool curAuto=false;
    int temperature=0;
    int rpm=0;
    double power=0;

signals:
    void updateMonitor(int index, int speed, int rpm, int temperature, double power);
};

class CpuFanController : public FanController {
Q_OBJECT

public:
    CpuFanController(ConfigManager *config, QObject *parent);
    ~CpuFanController();

protected:
    int getTemp();
    double getPower();

private:
    CpuPowerMonitor *cpuMonitor=nullptr;
};

class GpuFanController : public FanController {
Q_OBJECT

public:
    GpuFanController(ConfigManager *config, QObject *parent);

protected:
    int getTemp();
    double getPower();

private:
    bool shouldMonitorGpu();
};

#endif
