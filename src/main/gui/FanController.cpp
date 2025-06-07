#include "FanController.h"
#include "CFCmonitor.h"
#include "ConfigManager.h"

#include <QtCore/qprocess.h>
#include <QtCore/qdebug.h>
#include <QtCore/qdatetime.h>
#include <QtCore/qdir.h>
#include <QtCore/qtextstream.h>
#include <algorithm>
#include <atomic>
#include <numeric>
#include <qcontainerfwd.h>
#include <qcoreapplication.h>
#include <qimage.h>
#include <qlogging.h>
#include <qthread.h>
#include <iostream>

#ifdef _WIN32
#include "../winRing0Api.h"
#endif

#include "ExternalFan.h"

//for cpu temp in Windows
#define IA32_PACKAGE_THERM_STATUS_MSR 0x1B1

std::atomic_bool HardwareMonitor::shouldMonitorGpu=0;

void CpuPowerMonitor::rdmsr(int pos, char *dest) {
    memset(dest, 0, 8);
#ifdef __linux__
    try {
        FILE *fp=fopen(cpuMsrDir,"rb");
        if(fp==NULL)
            return;
        fseek(fp, pos, SEEK_SET);
        fread(dest,8,1,fp);
        fclose(fp);
        return;
    } catch(std::exception &exc) {
        return;
    }
#elif _WIN32
    DWORD eax=0;
	DWORD edx=0;
    Rdmsr(pos,&eax,&edx);
    memcpy(dest,&eax,4);
    memcpy(dest+4,&edx,4);
#endif
}

CpuPowerMonitor::CpuPowerMonitor(int index) {
    this->cpuIndex=index;
    this->lastQueryTime=std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    std::string msrDir="/dev/cpu/"+std::to_string(cpuIndex)+"/msr";
    strcpy(cpuMsrDir, msrDir.c_str());
    this->lastEnergy=getCurEnergy();
}

float CpuPowerMonitor::getCurEnergy() {
    char buff[8];
    rdmsr(MSR_RAPL_POWER_UNIT, buff);
    char times=0;
    memcpy(&times, buff+1, 1);
    rdmsr(MSR_PKG_ENERGY_STATUS, buff);
    uint32_t oriEnergy;
    memcpy(&oriEnergy,buff,4);
    float realEnergy=(float)oriEnergy*1000/std::pow(2,times); // in mwatt
    return realEnergy;
}

float CpuPowerMonitor::getPower() {
    long curTime=std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    float curEnergy=getCurEnergy();
    float pwr=(curEnergy-lastEnergy)/(curTime-lastQueryTime);
    lastQueryTime=curTime;
    lastEnergy=curEnergy;
    return pwr;
}

HardwareMonitor::HardwareMonitor(int index, ConfigManager *cfg, QObject *parent) : QThread(parent) {
    this->index=index;
    this->cfg=cfg;
    for (int i=0;i<cfg->fanProfiles[cfg->profileInUse].args[index-1].pwrCount;i++)
        this->lastPower.append(0.0);

    //record cpu temp file
#ifdef __linux__
    if (this->index==1) {
        bool found=false;
        const QString thermalRoot="/sys/class/thermal";
        QDir thermalRootDir(thermalRoot);
        QStringList zoneList=thermalRootDir.entryList({"thermal_zone*"},QDir::Dirs);
        for (auto &i : zoneList) {
            QFile curZoneType(thermalRoot+"/"+i+"/type");
            if (curZoneType.exists()) {
                if (curZoneType.open(QIODevice::ReadOnly|QIODevice::Text)) {
                    QTextStream stream(&curZoneType);
                    QString curType=stream.readAll().trimmed();
                    if (curType=="x86_pkg_temp") {
                        found=true;
                        this->cpuTempFile.setFileName(thermalRoot+"/"+i+"/temp");
                    }
                    curZoneType.close();
                }
                
            }
        }

        if (!found)
            throw "x86_pkg_temp not found";
        else
            this->cpuTempFile.open(QIODevice::ReadOnly|QIODevice::Text);
    }
#endif
}

void HardwareMonitor::run() {
    qDebug()<<"HardwareMonitor start fan: "<<index;

    if(index==1)
        this->cmonitor=new CpuPowerMonitor(0);

    while(!isInterruptionRequested()) {
        //get values
        if(index==1) {
            this->temperature=getcTemp();
            this->power=getcPower();
        } else if(index==2) {
            HardwareMonitor::shouldMonitorGpu=checkShouldMonitorGpu();
            if(HardwareMonitor::shouldMonitorGpu) {
                this->temperature=getgTemp();
                this->power=getgPower();
            } else {
                this->temperature=0;
                this->power=0;
            }
        }
        this->lastPower.append(this->power > 0 ? this->power.load() : 0);
        this->lastPower.removeFirst();
        emit requireUpdateMonitor2(this->index, this->temperature, this->power);
        QThread::msleep(cfg->monitorIntervals[this->index-1]);
    }

    if(index==1)
        delete this->cmonitor;

    qDebug()<<"HardwareMonitor finish fan: "<<index;
}

int HardwareMonitor::getcTemp() {
    //qDebug()<<"get cpu temp";
    int temperature=0;
#ifdef _WIN32
    DWORD eax=0;
	DWORD edx=0;
    Rdmsr(IA32_PACKAGE_THERM_STATUS_MSR,&eax,&edx);
    temperature=100-((eax & 0x007F0000) >> 16);
#elif __linux__
    QTextStream stream(&cpuTempFile);
    temperature=stream.readAll().trimmed().toInt()/1000;
    cpuTempFile.seek(0);
#endif
    //qDebug()<<"get cpu temp finish: "<<temperature;
    return temperature;
}

float HardwareMonitor::getcPower() {
    return this->cmonitor->getPower();
}

int HardwareMonitor::getgTemp() {
    QStringList list;
    try {
        list=nvsmiOutputParser({"-q","--display=TEMPERATURE"}, "GPU Current Temp");
        return list[0].trimmed().mid(0,list[0].size()-2).toInt();
    } catch (const char* exc) {
        return 0;
    }
}

float HardwareMonitor::getgPower() {
    try {
        float res[2];
        const char *flags[]={"Power Draw","Instantaneous Power Draw"};
        for (int i=0;i<2;i++) {
            QStringList list;
            list=nvsmiOutputParser({"-q","--display=POWER"}, flags[i]);
            if (list.size()==0)
                res[i]=0;
            else
                res[i]=list[0].trimmed().mid(0,list[0].size()-2).toFloat();
        }
        
        return std::max(res[0],res[1]);
    } catch (const char* exc) {
        return 0;
    }
}

QStringList HardwareMonitor::nvsmiOutputParser(QStringList args, QString flag) {
    QStringList result;
    QProcess nvsmi;
    nvsmi.start("nvidia-smi",args);
    nvsmi.waitForFinished();
    QString output=nvsmi.readAllStandardOutput();

    if(output.contains("NVIDIA-SMI has failed because it couldn't communicate with the NVIDIA driver.")) {
        throw "driver-error";
    }

    QStringList list=output.split('\n');
    for(auto curStr : list) {
        int index=curStr.indexOf(flag);
        if(index!=-1) {
            int index2=curStr.indexOf(":",index);
            result.push_back(curStr.mid(index2+2,curStr.size()-index2));
        }
    }
    return result;
}

bool HardwareMonitor::checkSysFile() {
    QFile sysFile(cfg->gpuSysDir);
    if(sysFile.exists()) {
        sysFile.open(QIODevice::ReadOnly);
        QString status=sysFile.readLine();
        sysFile.close();
        return status=="active\n";
    } else
        return false;
}

bool HardwareMonitor::checkNvsmiProc() {
    QStringList list;
    try {
        list=nvsmiOutputParser({"-q","--display=PIDS"}, "Name");
    } catch (const char* exc) {
        return false;
    }
    for(auto curStr : list) {
        if(curStr!="/usr/lib/Xorg")
            return true;
    }

    return false;
}

bool HardwareMonitor::checkShouldMonitorGpu() {
    if(cfg->monitorGpu) //when force enabled
        return true;

    //auto detect
#ifdef __linux__ //linux only, on windows will return 0
    if(cfg->gpuAutoDetectEnabled) {
        if(gpuCheckPaused) {
            if(QDateTime::currentSecsSinceEpoch()<=gpuCheckPauseTime+nvsmiPauseInterval)
                return false;
            else
                gpuCheckPaused=false;
        }

        if(!checkSysFile())
            return false;
        else {
            if(!checkNvsmiProc()) {
                gpuCheckPaused=true;
                gpuCheckPauseTime=QDateTime::currentSecsSinceEpoch();
            }
            return true;
        }
    }

    return false;
#elif _WIN32
    return false;
#endif
}

HardwareMonitor::~HardwareMonitor() {
    if (this->index==1)
        this->cpuTempFile.close();
}

FanController::FanController(ConfigManager *config, QObject *parent, int index, CFCmonitor *appMonitor, ExternalFan *exFan) : QThread(parent) {
    qDebug()<<"FanController general construct";
    this->index=index;
    this->config=config;
    this->appMonitor=appMonitor;
    this->exFan=exFan;

    this->hwMonitor=new HardwareMonitor(index,config,this);
    QObject::connect(this, &FanController::requireUpdateMonitor1, appMonitor, &CFCmonitor::updateValue1, Qt::BlockingQueuedConnection);
    QObject::connect(hwMonitor, &HardwareMonitor::requireUpdateMonitor2, appMonitor, &CFCmonitor::updateValue2, Qt::BlockingQueuedConnection);
}

FanController::~FanController() {
    qDebug()<<"FanController general deconstruct";
}

int FanController::getMinSpeed() {
    int result=0;
    fanArg *profileArgs=&(config->fanProfiles[config->profileInUse].args[index-1]);

    if (profileArgs->minSpeedList==nullptr)
        result=profileArgs->minSpeed;
    else {
        //avg
        float avgPower=std::accumulate(hwMonitor->lastPower.begin(),hwMonitor->lastPower.end(),0.0)/hwMonitor->lastPower.size();

        result=CfcUtils::calcTable(profileArgs->minSpeedList, profileArgs->minSpeed, avgPower);
    }

    return result;
}

void FanController::run() {
    qDebug()<<"FanController start fan: "<<index;
    this->hwMonitor->start();
    this->accessor.setFanSpeed(CfcDef::DEFAULT_SPEED, this->index);

    // loop
    while(!isInterruptionRequested()) {
        currentTime=QDateTime::currentMSecsSinceEpoch();
        fanArg *curProfileArgs=&(config->fanProfiles[config->profileInUse].args[index-1]);
        if(currentTime>lastControlTime+curProfileArgs->operateInterval) {
            // qDebug()<<"time to adjust fan: "<<index;

            rpm=getRpm();
            //first decide speed
            int targetSpeed=-10;
            if (config->useClevoAuto) //auto
                targetSpeed=-1;
            else if (config->useStaticSpeed) //static
                targetSpeed=std::clamp(config->staticSpeed[index-1],0,100);
            else if (config->maxSpeed) //max speed
                targetSpeed=100;
            else { //normal
                int curTemp=this->hwMonitor->temperature;
                if (curTemp>curProfileArgs->speedUpTemp)
                    targetSpeed=this->curSpeed+curProfileArgs->speedStep;
                else if (curTemp<curProfileArgs->slowDownTemp)
                    targetSpeed=this->curSpeed-curProfileArgs->speedStep;
                else
                    targetSpeed=curSpeed;
                targetSpeed=std::clamp(targetSpeed,getMinSpeed(),100);
            }

            //correct the data
            if (config->useSpeedLimit && targetSpeed!=-1)
                targetSpeed=std::clamp(targetSpeed,0,config->speedLimit[index-1]);

            //then apply speed
            if (curSpeed!=targetSpeed) {
                //auto
                // qDebug()<<"Apply fan spped index "<<index<<" "<<targetSpeed;
                if (targetSpeed==-1)
                    accessor.setFanSpeed(-1, index);
                else
                    accessor.setFanSpeed(targetSpeed, index);
                curSpeed=targetSpeed;
            }
            
            emit requireUpdateMonitor1(index,targetSpeed, rpm);

#ifdef CFC_USE_EX_FAN
            emit exFan->adjustFanSig(this->index,this->hwMonitor->power,config->maxSpeed);
#endif
            
            lastControlTime=currentTime;
        }
        QThread::msleep(CfcDef::MIN_CONTROL_INTERVAL);
    }
    accessor.setFanSpeed(-1,index); //finalize auto
    this->hwMonitor->requestInterruption();
    this->hwMonitor->wait();
    this->hwMonitor->quit();
    qDebug()<<"hwMonitor quit "<<index;
    qDebug()<<"FanController run finish "<<index;
}

int FanController::getRpm() {
    return accessor.getRpm(index);
}
