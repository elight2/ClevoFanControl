#include "ClevoEcAccessor.h"
#include <atomic>

#ifdef __linux__
#include <sys/io.h>
#include <unistd.h>
#endif

#ifdef _WIN32
#include "winRing0Api.h"
#endif

#include <QtCore/qdebug.h>

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

std::atomic_bool ClevoEcAccessor::ioInitialized=false;
std::mutex ClevoEcAccessor::EClock;
std::atomic_bool ClevoEcAccessor::havePrivilege=false;

ClevoEcAccessor::ClevoEcAccessor() {
    if(!ioInitialized) {
        qDebug()<<"EC accessor not init, init";

        //check privilege
#ifdef _WIN32
        havePrivilege=true;
#elif __linux__
        havePrivilege = getuid()==0 ? true : false;
#endif
        if(!havePrivilege)
            qWarning()<<"Checking privilege: The app may be running without necessary permissions!";

        //init IO
#ifdef _WIN32
        BOOL WinRing0result=winRing0Api::initApi();
        qDebug()<<"InitOpenLibSys result: "<<WinRing0result;
#elif __linux__
        ioperm(0x62, 1, 1);
        ioperm(0x66, 1, 1);
#endif
        ioInitialized=true;
    }
}

ClevoEcAccessor::~ClevoEcAccessor() {
    if(ioInitialized) {
        qDebug()<<"EC accessor not deinit, deinit";
#ifdef _WIN32
        BOOL WinRing0result=winRing0Api::deinitApi();
        qDebug()<<"DeinitOpenLibSys result: "<<WinRing0result;
#endif
        ioInitialized=false;
    }
}

void ClevoEcAccessor::setFanSpeed(int percentage, int index) {
    if(!havePrivilege) {
        qWarning()<<"No permission, setting fan speed aborted!";
        return;
    }
    if(percentage!=-1) //normal
        doEc(EC_SET_FAN_SPEED_CMD, index,percentage*255/100);
    else //auto
        doEc(EC_SET_FAN_SPEED_CMD, EC_SET_FAN_AUTO_ADDR, index);
}

int ClevoEcAccessor::getRpm(int index) {
    if(!havePrivilege) {
        qWarning()<<"No permission, getting fan RPM aborted!";
        return 0;
    }
	int data=0;
    if(index==1)
        data = (readEc_1(EC_CPU_FAN_RPM_HI_ADDR) << 8) + (readEc_1(EC_CPU_FAN_RPM_LO_ADDR));
    else if(index==2)
        data=(readEc_1(EC_GPU_FAN_RPM_HI_ADDR) << 8) + (readEc_1(EC_GPU_FAN_RPM_LO_ADDR));
    return data==0 ? 0 : (2156220 / data);
}

void ClevoEcAccessor::ioOutByte(unsigned short port, unsigned char value) {
#ifdef _WIN32
    WriteIoPortByte(port, value);
#elif __linux__
    outb(value, port);
#endif
	return;
}

unsigned char ClevoEcAccessor::ioInByte(unsigned short port) {
	unsigned char ret=0;
#ifdef _WIN32
	ret = ReadIoPortByte(port);
#elif __linux__
	ret = inb(port);
#endif
	return ret;
}

void ClevoEcAccessor::waitEc(unsigned short port, unsigned char index, unsigned char value) {
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

unsigned char ClevoEcAccessor::readEc_1(unsigned char addr) {
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

void ClevoEcAccessor::doEc(unsigned char cmd, unsigned char addr, unsigned char value) {
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