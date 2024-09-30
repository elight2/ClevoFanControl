中文
--------------
**使用本软件有风险，使用前请注意！建议先阅读功能介绍！作者对可能造成的损坏概不负责！**

此软件用来自动控制蓝天模具笔记本的风扇，大家可以做参考使用

### 原理及说明
* [关于Windows环境的问题](https://tieba.baidu.com/p/9101786783)
* [Linux环境及原理解释](https://tieba.baidu.com/p/9101786783)
* 仓库里的`ec-reference.zip`里面有我备份的参考资料

### 风扇控制接口
* Windows：[WinRing0](https://github.com/GermanAizek/WinRing0) [预编译文件](https://github.com/QCute/WinRing0)
* Linux：inb()和outb()

### 温度获取接口
* Windows：CPU使用MSR寄存器中的`IA32_PACKAGE_THERM_STATUS_MSR`，显卡使用`nvidia-smi`
* Linux：CPU使用`/sys/class/thermal/thermal_zone`，显卡使用`nvidia-smi`
*n卡驱动版本似乎必须是500以上才能正确读取温度*

### 功能介绍

#### 托盘
* Monitor：用于监视实时状态（转速读取可能有问题）
* Config：管理软件设置
* Profile: 用于选择使用的配置方案
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
---------
* 关于多配置功能：这个功能只能用于选择或编辑已有配置，我没有做添加/删除/重命名功能，如有需要请手动修改`config.json`

### 编译
* 工具：cmake，ninja，gcc，qt6
1. 下载源码，cd到源码目录
2. 配置：`cmake -B build -S . -G Ninja`，手动修改`CMakeLists.txt`里的路径（include，cmake和Windows上的qt自带gcc路径）和编译类型
3. cd到`build`，编译：`cmake --build .`

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
1. clone the source
2. configure the project: `cmake -B build -S . -G Ninja`
3. Modify directories and build type in `CMakeLists.txt`
4. in `build`, run `cmake --build .`
