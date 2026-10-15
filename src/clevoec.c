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

static void wait_ec(unsigned short port, unsigned char index, unsigned char value);
static unsigned char read_ec(unsigned char addr);
static void do_ec(unsigned char cmd, unsigned char addr, unsigned char value);

int ec_init(void) {
    if(ioperm(0x62, 1, 1)==-1)
        return -errno;
    if(ioperm(0x66, 1, 1)==-1)
        return -errno;
    return 0;
}

int set_fan_speed(int duty, int index) {
    if(index!=1 && index!=2)
        return -2;
    if(duty!=-1 && (duty<0 || duty>255)) //-1 is the auto sentinel
        return -1;
    if(duty!=-1) //normal
        do_ec(EC_SET_FAN_SPEED_CMD, index, duty);
    else //auto
        do_ec(EC_SET_FAN_SPEED_CMD, EC_SET_FAN_AUTO_ADDR, index);
    return 0;
}

int get_rpm(int index) {
	int data=0;
    if(index==1)
        data = (read_ec(EC_CPU_FAN_RPM_HI_ADDR) << 8) + (read_ec(EC_CPU_FAN_RPM_LO_ADDR));
    else if(index==2)
        data=(read_ec(EC_GPU_FAN_RPM_HI_ADDR) << 8) + (read_ec(EC_GPU_FAN_RPM_LO_ADDR));
    else
        return -2;
    return data==0 ? 0 : (2156220 / data);
}

static void wait_ec(unsigned short port, unsigned char index, unsigned char value) {
	unsigned char data=0;
	int retry=100; //timeout, ~100ms
	do
	{
		data=inb(port);
		usleep(1000);
	}while(((data>>index)&((unsigned char)1))!=value && --retry>0);
	return;
}

static unsigned char read_ec(unsigned char addr) {
    wait_ec(EC_SC_REG,EC_SC_IBF_INDEX,0);
    outb(EC_READ_CMD, EC_SC_REG);
    wait_ec(EC_SC_REG,EC_SC_IBF_INDEX,0);
    outb(addr, EC_DATA_REG);
    wait_ec(EC_SC_REG,EC_SC_OBF_INDEX,1);
    unsigned char value=0;
    value=inb(EC_DATA_REG);
	return value;
}

static void do_ec(unsigned char cmd, unsigned char addr, unsigned char value) {
    wait_ec(EC_SC_REG,EC_SC_IBF_INDEX,0);//not in doc, but for safety reason
    outb(cmd, EC_SC_REG);
    wait_ec(EC_SC_REG,EC_SC_IBF_INDEX,0);
    outb(addr, EC_DATA_REG);
    wait_ec(EC_SC_REG,EC_SC_IBF_INDEX,0);
    outb(value, EC_DATA_REG);
    wait_ec(EC_SC_REG,EC_SC_IBF_INDEX,0);
	return;
}
