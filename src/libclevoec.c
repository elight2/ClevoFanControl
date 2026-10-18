#include "libclevoec.h"
#include <sys/io.h>
#include <stdio.h>
#include <unistd.h>

// all of the dutys in 0-255
// all of the fan index in 1 2 3...

// communication protocol with EC is defined by ACPI
#define EC_SC_PORT 0x66 // status/command port, reading or writing operates different ram data, in the same port
#define EC_SC_OBF_INDEX 0 // defined in acpi spec 6.5 12.2.1
#define EC_SC_IBF_INDEX 1 // defined in acpi spec 6.5 12.2.1
#define EC_SC_CMD_INDEX 3 // defined in acpi spec 6.5 12.2.1
#define EC_DATA_PORT 0x62 // port for transfer data
#define EC_READ_CMD 0x80 // defined in acpi spec 6.5 12.3
#define EC_WRITE_CMD 0x81 // defined in acpi spec 6.5 12.3
// following addrs are dumped from DSDT
#define EC_FCMD 0xF8
#define EC_FDAT 0xF9
#define EC_XFAN 0xE7
// analyzed by reverse engineering
#define EC_CLEVO_FAN_FIX_CMD 0x99
#define EC_CLEVO_FAN_RELEASE_CMD 0xff
#define EC_CLEVO_SWITCH_FAN_MODE_CMD 0xd7
// name from BIOS and addr analyzed from /sys/kernel/debug/ec/ec0/io
#define EC_RPM1 0xD0
#define EC_RPM2 0xD2

// get io perm
int init_perm() {
    int ret = 0;
    if (ret = ioperm(EC_SC_PORT, 1, 1))
        return ret;
    if (ret = ioperm(EC_DATA_PORT, 1, 1))
        return ret;
    return ret;
}

// wait single bit in EC_SC to flag
// to prevent data corruption
void wait_ec(unsigned char status_reg_index, unsigned char flag_value) {
    unsigned char data = inb(EC_SC_PORT);
    while (((data >> status_reg_index) & 1) != flag_value) {
        data = inb(EC_SC_PORT);
        usleep(1);
    }
}

//read single byte from EC RAM
unsigned char read_ec_ram(unsigned char addr) {
    wait_ec(EC_SC_IBF_INDEX, 0);
    outb(EC_READ_CMD, EC_SC_PORT);
    wait_ec(EC_SC_IBF_INDEX, 0);
    outb(addr, EC_DATA_PORT);
    wait_ec(EC_SC_IBF_INDEX, 0);
    wait_ec(EC_SC_OBF_INDEX, 1);
    return inb(EC_DATA_PORT);
}

// write single byte to EC RAM
void write_ec_ram(unsigned char addr, unsigned char value) {
    wait_ec(EC_SC_IBF_INDEX, 0);
    outb(EC_WRITE_CMD, EC_SC_PORT);
    wait_ec(EC_SC_IBF_INDEX, 0);
    outb(addr, EC_DATA_PORT);
    wait_ec(EC_SC_IBF_INDEX, 0);
    outb(value, EC_DATA_PORT);
    wait_ec(EC_SC_IBF_INDEX, 0);
    return;
}

// get physical fan RPM
// analyzed from /sys/kernel/debug/ec/ec0/io
// RPM algorithm from IT5570E datasheet 7.12.3.2
unsigned short get_rpm(unsigned char rpm_addr) {
    unsigned short data = (read_ec_ram(rpm_addr) << 8) | read_ec_ram(rpm_addr + 1);
    return data == 0 ? 0 : (2156250 / data);
}

// set fan to a special fixed mode and give it a fixed duty
// analyzed by reverse engineering
void fix_fan_duty(unsigned char fan_index, unsigned char duty) {
    wait_ec(EC_SC_IBF_INDEX, 0);
    outb(EC_CLEVO_FAN_FIX_CMD, EC_SC_PORT);
    wait_ec(EC_SC_IBF_INDEX, 0);
    outb(fan_index, EC_DATA_PORT);
    wait_ec(EC_SC_IBF_INDEX, 0);
    outb(duty, EC_DATA_PORT);
    wait_ec(EC_SC_IBF_INDEX, 0);
    return;
}

// analyzed by reverse engineering
// release fan from fixed mode
void release_fan_duty(unsigned char fan_index) {
    wait_ec(EC_SC_IBF_INDEX, 0);
    outb(EC_CLEVO_FAN_FIX_CMD, EC_SC_PORT);
    wait_ec(EC_SC_IBF_INDEX, 0);
    outb(EC_CLEVO_FAN_RELEASE_CMD, EC_DATA_PORT);
    wait_ec(EC_SC_IBF_INDEX, 0);
    outb(fan_index, EC_DATA_PORT);
    wait_ec(EC_SC_IBF_INDEX, 0);
    return;
}

// switch fan between 3 normal mode (3 modes in Clevo Control Center 3.0)
// mode in EC_CLEVO_FAN_MODE_xxx
void set_fan_mode(unsigned char mode) {
    write_ec_ram(EC_FDAT, mode);
    write_ec_ram(EC_FCMD, EC_CLEVO_SWITCH_FAN_MODE_CMD);
    return;
}

// "offset" parameter in Clevo CC's "Auto" mode
// invalid in other mode
// offset in 0-255
void set_fan_offset(unsigned char offset) {
    write_ec_ram(EC_XFAN, offset);
    return;
}

