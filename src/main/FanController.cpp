#include "FanController.h"
#include "src/main/ConfigManager.h"
#include <mutex>
#include <qcontainerfwd.h>
#include <qdebug.h>
#include <qimage.h>
#include <qlogging.h>
#include <qobject.h>
#include <qthread.h>
#include <unistd.h>

#define IA32_PACKAGE_THERM_STATUS_MSR 0x1B1

//std EC
#define EC_SC_REG 0x66
#define EC_DATA_REG 0x62
#define EC_READ_CMD 0x80
#define EC_WRITE_CMD 0x81
#define EC_SC_IBF_INDEX 1
#define EC_SC_OBF_INDEX 0
//clevo
#define EC_CPU_FAN_RPM_HI_ADDR 0xD0
#define EC_CPU_FAN_RPM_LO_ADDR 0xD1
#define EC_GPU_FAN_RPM_HI_ADDR 0xD2
#define EC_GPU_FAN_RPM_LO_ADDR 0xD3
#define EC_SET_FAN_SPEED_CMD 0x99
#define EC_SET_FAN_AUTO_ADDR 0xFF

#define C_PROFILE_INDEX 0
#define G_PROFILE_INDEX 1

std::atomic_bool FanController::ioInitialized=false;
std::mutex FanController::EClock;
#ifdef _WIN32
HMODULE FanController::WinRing0m=NULL;
bool FanController::InitOpenLibSys_m() {
    return InitOpenLibSys(&WinRing0m);
}

bool FanController::DeinitOpenLibSys_m() {
    return DeinitOpenLibSys(&WinRing0m);
}
#endif

void FanController::ioOutByte(unsigned short port, unsigned char value) {
#ifdef _WIN32
    WriteIoPortByte(port, value);
#elif __linux__
    outb(value, port);
#endif
	return;
}

unsigned char FanController::ioInByte(unsigned short port) {
	unsigned char ret=0;
#ifdef _WIN32
	ret = ReadIoPortByte(port);
#elif __linux__
	ret = inb(port);
#endif
	return ret;
}

void FanController::waitEc(unsigned short port, unsigned char index, unsigned char value) {
	unsigned char data=0;
	do
	{
		data=ioInByte(port);
#ifdef __linux__
		usleep(1000);
#endif
	}while(((data>>index)&((unsigned char)1))!=value);
	return;
}

// unsigned char readEc(unsigned char addr)
// {
//     //qDebug()<<"read ec";
//     EClock.lock();
// 	waitEc(EC_SC_REG,EC_SC_IBF_INDEX,0);
// 	ioOutByte(EC_SC_REG,EC_READ_CMD);
// 	waitEc(EC_SC_REG,EC_SC_IBF_INDEX,0);
// 	ioOutByte(EC_DATA_REG,addr);
// 	waitEc(EC_SC_REG,EC_SC_OBF_INDEX,1);
// 	unsigned char value=0;
// 	value=ioInByte(EC_DATA_REG);
//     //qDebug()<<"read ec finish";
//     EClock.unlock();
// 	return value;
// }

unsigned char FanController::readEc_1(unsigned char addr) {
    //qDebug()<<"read ec";
    EClock.lock();
    waitEc(EC_SC_REG,EC_SC_IBF_INDEX,0);
    ioOutByte(EC_SC_REG,EC_READ_CMD);
    waitEc(EC_SC_REG,EC_SC_IBF_INDEX,0);
    ioOutByte(EC_DATA_REG,addr);
#ifdef _WIN32
    Sleep(1);
#elif __linux__
    waitEc(EC_SC_REG,EC_SC_OBF_INDEX,1);
#endif
    unsigned char value=0;
    value=ioInByte(EC_DATA_REG);
    //qDebug()<<"read ec finish";
    EClock.unlock();
	return value;
}

// void writeEc(unsigned char addr, unsigned char value)
// {
// 	waitEc(EC_SC_REG,EC_SC_IBF_INDEX,0);
// 	ioOutByte(EC_SC_REG,EC_WRITE_CMD);
// 	waitEc(EC_SC_REG,EC_SC_IBF_INDEX,0);
// 	ioOutByte(EC_DATA_REG,addr);
// 	waitEc(EC_SC_REG,EC_SC_IBF_INDEX,0);
// 	ioOutByte(EC_DATA_REG,value);
// 	waitEc(EC_SC_REG,EC_SC_IBF_INDEX,0);
// 	return;
// }

void FanController::doEc(unsigned char cmd, unsigned char addr, unsigned char value) {
    EClock.lock();
    waitEc(EC_SC_REG,EC_SC_IBF_INDEX,0);//not in doc, but for safety reason
    ioOutByte(EC_SC_REG,cmd);
    waitEc(EC_SC_REG,EC_SC_IBF_INDEX,0);
    ioOutByte(EC_DATA_REG,addr);
    waitEc(EC_SC_REG,EC_SC_IBF_INDEX,0);
    ioOutByte(EC_DATA_REG,value);
    waitEc(EC_SC_REG,EC_SC_IBF_INDEX,0);
    EClock.unlock();
	return;
}

void FanController::setFanSpeed(int percentage) {
    if(!havePrivilege) {
        qWarning()<<"No permission, setting fan speed aborted!";
        return;
    }
    if(percentage!=-1) //normal
        doEc(EC_SET_FAN_SPEED_CMD, index,percentage*255/100);
    else //auto
        doEc(EC_SET_FAN_SPEED_CMD, EC_SET_FAN_AUTO_ADDR, index);
}

FanController::FanController(ConfigManager *configMgr, QObject *parent) : QThread(parent) {
    qDebug()<<"FanController general construct";
    config=configMgr;

    //check privilege
#ifdef _WIN32
    havePrivilege=1;
#elif __linux__
    havePrivilege = getuid()==0 ? 1 : 0;
#endif
    if(!havePrivilege)
        qWarning()<<"The app may be running without necessary permissions!";
    
    //init io
    if(!ioInitialized) {
        qDebug()<<"not init, init: "<<index;
#ifdef _WIN32
        bool WinRing0result=InitOpenLibSys_m();
        qDebug()<<"InitOpenLibSys result: "<<WinRing0result;
#elif __linux__
        ioperm(0x62, 1, 1);
        ioperm(0x66, 1, 1);
#endif
        ioInitialized=true;
    }
}

FanController::~FanController() {
    qDebug()<<"FanController general deconstruct";
    if(ioInitialized) {
        qDebug()<<"not deinit, deinit: "<<index;
#ifdef _WIN32
        bool WinRing0result=DeinitOpenLibSys_m();
        qDebug()<<"DeinitOpenLibSys result: "<<WinRing0result;
#endif
        ioInitialized=false;
    }
}

void FanController::setShouldStop() {
    shouldRun=0;
    while(isRunning)
        QThread::msleep(100);
}

void FanController::run() {
    qDebug()<<"FanController run fan: "<<index;
    isRunning=1;
    while(shouldRun)
    {
        currentTime=QDateTime::currentMSecsSinceEpoch();
        if(currentTime>lastControlTime+config->timeIntervals[index-1])
        {
            //qDebug()<<"controling speed fan: "<<index;
            int targetSpeed=-1;

            if(config->useClevoAuto) {
                targetSpeed=-2;
                qDebug()<<"adjust fan: "<<index<<" auto";
            }
            else if(config->maxSpeed) {
                targetSpeed=100;
                //qDebug()<<"adjust fan: "<<index<<" max";
            }
            else if(config->useStaticSpeed) {
                targetSpeed=config->staticSpeed[index-1];
                //qDebug()<<"adjust fan: "<<index<<" static"<<targetSpeed;
            }
            else {
                temperature = getTemp();
                int mode=config->fanProfiles[config->profileInUse].inUse[index-1];
                targetSpeed=curSpeed;
                if(mode==1) {
                    if(temperature>(config->fanProfiles[config->profileInUse].MTconfig[index-1][0]))//> max
                        targetSpeed+=config->fanProfiles[config->profileInUse].MTconfig[index-1][3];//+=step
                    else if(temperature<(config->fanProfiles[config->profileInUse].MTconfig[index-1][1]))//<min
                        targetSpeed-=config->fanProfiles[config->profileInUse].MTconfig[index-1][3];//-=step
                    targetSpeed=std::max({targetSpeed,minSafeSpeed,config->fanProfiles[config->profileInUse].MTconfig[index-1][2]});
                    targetSpeed=std::min(targetSpeed,100);
                }
                else if(mode==2) {
                    targetSpeed=config->fanProfiles[config->profileInUse].TSspeedList[index-1][0];
                    if(temperature<(config->fanProfiles[config->profileInUse].TStempList[index-1][0]))
                        targetSpeed=config->fanProfiles[config->profileInUse].TSspeedList[index-1][0];
                    else {
                        for(int i=0;i<10;i++)
                        {
                            if(i==9)
                                targetSpeed=config->fanProfiles[config->profileInUse].TSspeedList[index-1][9];
                            else if(temperature>=(config->fanProfiles[config->profileInUse].TStempList[index-1][i]) && temperature<(config->fanProfiles[config->profileInUse].TStempList[index-1][i+1]))
                            {
                                targetSpeed=config->fanProfiles[config->profileInUse].TSspeedList[index-1][i];
                                break;
                            }
                        }
                    }
                    targetSpeed=std::max(targetSpeed,minSafeSpeed);
                    targetSpeed=std::min(targetSpeed,100);
                }
                if(config->useSpeedLimit)
                    targetSpeed=std::min(targetSpeed,config->speedLimit[index-1]);
                //qDebug()<<"adjust fan: "<<index<<" normal"<<targetSpeed;
            }

            if(targetSpeed==-2) { //auto
                if(!curAuto) {
                    setFanSpeed(-1);
                    curAuto=true;
                }
            }
            else {
                curAuto=false;
                if(curSpeed!=targetSpeed) {
                    qDebug()<<"apply speed: "<<index<<" "<<targetSpeed;
                    setFanSpeed(targetSpeed);
                    qDebug()<<"apply speed finish: "<<index<<" "<<targetSpeed;
                    curSpeed=targetSpeed;
                }
            }
            if(index==1) //cpu
                rpm=getRpm();
            else if(index==2) //gpu
                rpm=getRpm();

            emit updateMonitor(index,targetSpeed, rpm, temperature);

            if(rpm==0)
                curSpeed=0;//toggle speed adjust
            lastControlTime=currentTime;
        }
        QThread::msleep(100);
    }
    setFanSpeed(index); //finalize auto
    isRunning=false;
    qDebug()<<"FanController run finish "<<index;
}

CpuFanController::CpuFanController(ConfigManager *configMgr, QObject *parent) : FanController(configMgr, parent) {
    index=1;
}

int CpuFanController::getTemp() {
    //qDebug()<<"get cpu temp";
    int temperature=0;
#ifdef _WIN32
    DWORD eax=0;
	DWORD edx=0;
    Rdmsr(IA32_PACKAGE_THERM_STATUS_MSR,&eax,&edx);
    temperature=100-((eax & 0x007F0000) >> 16);
#elif __linux__
    QProcess bash;
    bash.start("bash",{"-c","paste <(cat /sys/class/thermal/thermal_zone*/type) <(cat /sys/class/thermal/thermal_zone*/temp) |grep x86_pkg_temp"});
    bash.waitForFinished();
    QString output=bash.readAllStandardOutput();
    QString tempStr=output.mid(13, output.size() - 13);
    temperature=tempStr.toInt();
    temperature/=1000;
#endif
    //qDebug()<<"get cpu temp finish: "<<temperature;
    return temperature;
}

int CpuFanController::getRpm() {
    if(!havePrivilege) {
        qWarning()<<"No permission, getting cpu fan speed aborted!";
        return 0;
    }
	int data = ((readEc_1(EC_CPU_FAN_RPM_HI_ADDR) << 8) + (readEc_1(EC_CPU_FAN_RPM_LO_ADDR)));
	return data==0 ? 0 : (2156220 / data);
}

GpuFanController::GpuFanController(ConfigManager *configMgr, QObject *parent) : FanController(configMgr, parent) {
    index=2;
}

int GpuFanController::getTemp() {
    //qDebug()<<"get gpu temp";
    int temperature=0;
    bool check=false;
    if(config->monitorGpu)
        check=true;
    else {
#ifdef  __linux__
        if(config->gpuAutoDetectEnabled) {
            QProcess lsof;
            lsof.start("lsof",{config->gpuDevDir});
            lsof.waitForFinished();
            QString output=lsof.readAllStandardOutput();
            QStringList list=output.split('\n');
            for(int i=1;i<list.size();i++) {
                int index=list[i].indexOf(' ');
                QString proc=list[i].mid(0,index);
                if(proc != "" && proc != "Xorg" && proc != "nvidia-sm") {
                    check=true;
                    break;
                }
            }
        }
#endif
    }

    if(check) {
        QProcess nvsmi;
        nvsmi.start("nvidia-smi",{"-q","-d=TEMPERATURE"});
        nvsmi.waitForFinished();
        QString output=nvsmi.readAllStandardOutput();
        int index=output.indexOf(static_cast<QString>("GPU Current Temp"));
        int index2=output.indexOf(static_cast<QString>(":"),index);
        int index3=output.indexOf(static_cast<QString>(" "),index2+2);
        temperature=output.mid(index2+2,index3-index2-2).toInt();
    }
    //qDebug()<<"get gpu temp finish: "<<temperature;
    return temperature;
}

int GpuFanController::getRpm() {
    if(!havePrivilege) {
        qWarning()<<"No permission, getting gpu fan speed aborted!";
        return 0;
    }
	int data = ((readEc_1(EC_GPU_FAN_RPM_HI_ADDR) << 8) + (readEc_1(EC_GPU_FAN_RPM_LO_ADDR)));
	return data==0 ? 0 : (2156220 / data);
}
