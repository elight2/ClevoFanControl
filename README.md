中文
--------------
**使用本软件有风险，使用前请注意！建议先阅读功能介绍！作者对可能造成的损坏概不负责！**

如果使用中遇到bug，欢迎提[issue](https://github.com/elight2/ClevoFanControl/issues)

此软件用来自动控制蓝天模具笔记本的风扇，大家可以做参考使用

建议了解一些json文件的语法，因为本软件的配置界面功能有限，一些功能需要手动编辑json文件来配置

### 原理及说明
* [关于Windows环境的问题](https://tieba.baidu.com/p/9101786783)
* [Linux环境及原理解释](https://tieba.baidu.com/p/9101786783)
* 仓库里的`ec.zip`里面有我备份的参考资料

#### 风扇控制接口
* Windows：[WinRing0](https://github.com/GermanAizek/WinRing0) [预编译文件](https://github.com/QCute/WinRing0)
* Linux：inb()和outb()

#### 温度获取接口
* Windows：CPU使用MSR寄存器中的`IA32_PACKAGE_THERM_STATUS_MSR`，显卡使用`nvidia-smi`
* Linux：CPU使用`/sys/class/thermal/thermal_zone`，显卡使用`nvidia-smi`
*n卡驱动版本似乎必须是500以上才能正确读取温度*

### 功能介绍

#### 托盘
* Monitor：用于监视实时状态（转速读取可能有问题）
* Config：管理软件设置
* Profile: 用于选择使用的配置方案
* Commands: 用于执行自定义的命令
* Max Speed：全速模式（强冷）
* Speed Limit：开启限速功能（设置在Config界面）
* Static Speed：开启设定转速功能（设置在Config界面）
* Clevo Auto：使用蓝天默认的“自动”模式
* Monitor GPU: 开启GPU温度检测
* Exit：退出，并使用蓝天默认的“自动”模式

#### 配置界面（Config）
* Profile: 可以创建不同的配置方案，显示的内容为正在编辑（不是正在使用的！只能在托盘选择使用的配置）的配置
* Temp-Speed模式：若温度处在温度列表（Temp栏）的两个值直接，则设置转速为较低温度对应的转速值（Speed栏）
* Max Temp模式：若温度高于"Speed Up Temp"，则风扇转速百分比提高"Speed Step"，若低于"Slow Down Temp"，则降低"Speed Step"，最低不低于"Min Speed"。**注意：这种模式可以获得较低的转速，并保证合适的核心温度，但会使主板温度升高，请小心使用**
* Static Speed：指定风扇转速百分比，主要用于调试风扇，勾选"enabled"来开启
* Speed Limit：限速功能，开启后Temp-Speed和Max Temp模式的转速都不会超过限制值
* 两个Interval值：检测核心温度并设定风扇转速的时间间隔，单位是毫秒。**注意：设定过低（即调速频率过高）可能会导致EC芯片出现问题**
* Clevo Auto：使用蓝天默认的“自动”模式
* Max Speed：全速模式（强冷）
* Apply按钮：应用设置并保存配置文件
* OK按钮：应用设置并保存配置文件，然后关闭界面

#### Commands功能
* 此功能本身与风扇调速无关，但我发现对于使用Linux的蓝天电脑很有用
* 主要目的是一键以root身份执行命令，来实现更好的性能和功耗调控，比如使用`nvidia-smi -lgc`或者`cpupower`

#### GPU温度检测功能
* 
* Windows下只能使用手动检测：不用显卡时关闭托盘的GPU温度检测开关，使用时手动打开
* Linux下打开托盘的GPU温度检测开关后强制检测温度
* Linux下的`config.json`中的`gpuAutoDetect`设置为`false`时（默认状态）等同Windows
* `gpuAutoDetect`为`true`时，根据`/sys`和`/dev`下的给定文件自动检测显卡是否开启，来决定是否使用`nvidia-smi`获取显卡温度，具体机制参考代码中的`GpuFanController::getTemp`

#### 特殊说明(重要)
* 关于多配置功能：这个功能只能用于选择或编辑已有配置，目前没有添加/删除/重命名配置的功能，如有需要请手动修改`config.json`
* 关于自定义命令: 此功能目前只能执行已经在`config.json`中保存的命令，要添加/删除/重命名命令，需要手动编辑文件，预置的两个命令用于在使用`acpi-cpufreq`驱动的Linux系统下设置CPU频率调度方案
----------
**Linux用户须知**
1. 直接使用root身份启动程序可能出现程序正常运行但是托盘图标看不到的情况，请使用`cfc-launcher.sh`启动，本质是`unset XDG_CURRENT_DESKTOP`
2. 软件需要qt的lib才能运行，需要安装libqt6相关文件
3. 自启动方案：编写一个`.desktop`文件，执行文件填一个sh脚本，脚本里面使用sudo启动本软件的主程序，把.desktop添加到xdg自启动，然后在`sudoers`里设置本程序无密码

### 编译
* 工具：cmake，ninja，gcc，qt6
1. 下载源码，cd到源码目录
2. 修改仓库中`src/scripts/`下的windows和linux的构建脚本，修改cmake配置选项，执行即可

English
--------------
**Modifying Hardware is risky, the Author is not responsible for any damage caused by this software!**

This software is used to control Clevo laptops' fan.

### mechanism
* `ec-reference.zip` in this repository contains all the referance resource I found.
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
