#ifndef LIBCLEVOEC_H
#define LIBCLEVOEC_H
#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

// Fan modes understood by set_fan_mode, matching Clevo Control Center 3.0.
#define EC_CLEVO_FAN_MODE_AUTO 0x02
#define EC_CLEVO_FAN_MODE_MAX 0x10
#define EC_CLEVO_FAN_MODE_CUSTOM 0x04

// ECU RAM addresses of the fan tachometers, one per fan index.
// A caller reading RPM needs the address, because get_rpm() takes an address.
#define EC_RPM1 0xD0 // fan 1, usually the CPU fan
#define EC_RPM2 0xD2 // fan 2, usually the GPU fan

// Grants the process access to the EC ports through ioperm().
// Needs root (CAP_SYS_RAWIO). Returns 0 on success, -1 on failure with errno set.
// Port access is per thread: call this on the thread that will do the I/O, before
// any worker inherits the port bitmap.
int init_perm(void);

// Reads one byte of EC RAM.
unsigned char read_ec_ram(unsigned char addr);

// Writes one byte of EC RAM.
void write_ec_ram(unsigned char addr, unsigned char value);

// Physical fan RPM. rpm_addr is EC_RPM1 or EC_RPM2; the high byte is at
// rpm_addr and the low byte at rpm_addr + 1.
unsigned short get_rpm(unsigned char rpm_addr);

// Puts fan fan_index (1, 2, ...) into fixed duty mode at duty (0-255).
void fix_fan_duty(unsigned char fan_index, unsigned char duty);

// Releases fan fan_index (1, 2, ...) back to the EC's own control.
void release_fan_duty(unsigned char fan_index);

// Switches the EC fan profile. mode is one of the EC_CLEVO_FAN_MODE_* values.
void set_fan_mode(unsigned char mode);

// Sets the "offset" parameter used by Clevo Control Center's "Auto" mode.
// Has no effect in any other mode. offset is 0-255.
void set_fan_offset(unsigned char offset);

#ifdef __cplusplus
}
#endif // __cplusplus
#endif // !LIBCLEVOEC_H
