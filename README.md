# ClevoFanControl

Clevo laptop fan control utility

# English version of this article is still under construction 

[简体中文](README_zh_cn.md)

**Modifying Hardware is risky, the Author is not responsible for any damage caused by this software!**

This software is used to control Clevo laptops' fan.

### mechanism
* `ec.zip` in this repository contains all the referance resource I found.
#### Fan control interface
* Windows: [WinRing0](https://github.com/GermanAizek/WinRing0) [pre-compiled binaries](https://github.com/QCute/WinRing0)
* Linux: `inb()` and `outb()`
#### Temperature query interface
* Windows: CPU: MSR register `IA32_PACKAGE_THERM_STATUS_MSR`, GPU: `nvidia-smi -q -d=TEMPERATURE`
* Linux: CPU: `/sys/class/thermal/thermal_zone*/temp`, GPU: `nvidia-smi -q -d=TEMPERATURE`
*It seems that nvidia driver version must be higher than 5xx.xx to correctly read GPU temp*

### Build
* Tools: cmake, ninja, gcc, qt6
1. clone the repository
2. Modify the build scrips in `src/scripts/`, make sure the cmake configure options are correct, finally run it
