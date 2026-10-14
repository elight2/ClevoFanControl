#include "clevoec.h"
#include <sys/io.h>
#include <unistd.h>
#include <errno.h>

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

static void ioOutByte(unsigned short port, unsigned char value);
static unsigned char ioInByte(unsigned short port);
static void waitEc(unsigned short port, unsigned char index, unsigned char value);
static unsigned char readEc_1(unsigned char addr);
static void doEc(unsigned char cmd, unsigned char addr, unsigned char value);

int init() {
    if(ioperm(0x62, 1, 1)==-1)
        return -errno;
    if(ioperm(0x66, 1, 1)==-1)
        return -errno;
    return 0;
}

int set_fan_speed(int duty, int index) {
    if(duty!=-1) //normal
        doEc(EC_SET_FAN_SPEED_CMD, index,duty);
    else //auto
        doEc(EC_SET_FAN_SPEED_CMD, EC_SET_FAN_AUTO_ADDR, index);
    return 0;
}

int get_rpm(int index) {
	int data=0;
    if(index==1)
        data = (readEc_1(EC_CPU_FAN_RPM_HI_ADDR) << 8) + (readEc_1(EC_CPU_FAN_RPM_LO_ADDR));
    else if(index==2)
        data=(readEc_1(EC_GPU_FAN_RPM_HI_ADDR) << 8) + (readEc_1(EC_GPU_FAN_RPM_LO_ADDR));
    return data==0 ? 0 : (2156220 / data);
}

static void ioOutByte(unsigned short port, unsigned char value) {
    outb(value, port);
	return;
}

static unsigned char ioInByte(unsigned short port) {
	unsigned char ret=0;
	ret = inb(port);
	return ret;
}

static void waitEc(unsigned short port, unsigned char index, unsigned char value) {
	unsigned char data=0;
	int retry=100; //timeout, ~100ms
	do
	{
		data=ioInByte(port);
		usleep(1000);
	}while(((data>>index)&((unsigned char)1))!=value && --retry>0);
	return;
}

// unsigned char readEc(unsigned char addr)
// {
//     //qDebug()<<"read ec";
// 	waitEc(EC_SC_REG,EC_SC_IBF_INDEX,0);
// 	ioOutByte(EC_SC_REG,EC_READ_CMD);
// 	waitEc(EC_SC_REG,EC_SC_IBF_INDEX,0);
// 	ioOutByte(EC_DATA_REG,addr);
// 	waitEc(EC_SC_REG,EC_SC_OBF_INDEX,1);
// 	unsigned char value=0;
// 	value=ioInByte(EC_DATA_REG);
//     //qDebug()<<"read ec finish";
// 	return value;
// }

// modified version
static unsigned char readEc_1(unsigned char addr) {
    waitEc(EC_SC_REG,EC_SC_IBF_INDEX,0);
    ioOutByte(EC_SC_REG,EC_READ_CMD);
    waitEc(EC_SC_REG,EC_SC_IBF_INDEX,0);
    ioOutByte(EC_DATA_REG,addr);
    waitEc(EC_SC_REG,EC_SC_OBF_INDEX,1);
    unsigned char value=0;
    value=ioInByte(EC_DATA_REG);
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

static void doEc(unsigned char cmd, unsigned char addr, unsigned char value) {
    waitEc(EC_SC_REG,EC_SC_IBF_INDEX,0);//not in doc, but for safety reason
    ioOutByte(EC_SC_REG,cmd);
    waitEc(EC_SC_REG,EC_SC_IBF_INDEX,0);
    ioOutByte(EC_DATA_REG,addr);
    waitEc(EC_SC_REG,EC_SC_IBF_INDEX,0);
    ioOutByte(EC_DATA_REG,value);
    waitEc(EC_SC_REG,EC_SC_IBF_INDEX,0);
	return;
}
