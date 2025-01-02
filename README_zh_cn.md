**使用本软件有风险，使用前请注意！建议先阅读功能介绍！作者对可能造成的损坏概不负责！**

此软件用来自动控制蓝天（主要是神舟）笔记本的风扇

如果使用中遇到bug，欢迎提[issue](https://github.com/elight2/ClevoFanControl/issues)

## 使用说明

**详细说明参考[功能介绍](#功能介绍)**

### Windows

1. 下载release里面对应的压缩包并解压
2. 解压，管理员权限运行`ClevoFanControl-gui`

### Linux

1. 下载release里面对应的压缩包并解压
2. 确保安装了Qt6的lib文件，或者确保软件能找到对应的`.so`；如果使用`ClevoFanControl-cmd`则不需要
3. root身份运行`ClevoFanControl-gui`

* Linux下直接使用root身份启动程序可能出现程序正常运行但是托盘图标看不到的情况，请使用`cfc-launcher.sh`启动，本质是`unset XDG_CURRENT_DESKTOP`
* 负载高（比如玩游戏）时请在托盘里切换`game***`模式，防止过热
* 使用显卡时请打开托盘里的`Monitor GPU`，否则不会检测显卡温度，参考[显卡温度检测说明](#显卡温度检测说明)
* 默认的配置文件是我在n960kp（神舟tx8）上根据自己的使用习惯测试并编写的，可能不适用于其他机型/用户，强烈建议阅读[功能介绍](#功能介绍)并自行修改配置

## 功能介绍

### 系统托盘

* Monitor：用于监视实时状态（转速读取可能有问题）
* Config：管理软件设置（这个做的不太完善，建议手动编辑配置文件）
* Profile: 用于选择使用的配置方案
* Commands: 用于执行自定义的命令
* Max Speed：全速模式（模拟蓝天强冷）
* Speed Limit：开启限速功能（设置在Config界面）
* Static Speed：开启恒定转速功能（设置在Config界面）
* Clevo Auto：使用蓝天默认的“自动”模式
* Monitor GPU: 开启GPU温度检测
* Exit：退出，并使用蓝天默认的“自动”模式

### 配置

* 关于多配置功能：这个功能只能用于选择或编辑已有配置，目前没有添加/删除/重命名配置的功能，如有需要请手动修改`config.json`
* 关于自定义命令: 此功能目前只能执行已经在`config.json`中保存的命令，要添加/删除/重命名命令，需要手动编辑文件，预置的两个命令用于在使用`acpi-cpufreq`驱动的Linux系统下设置CPU频率调度方案
* 默认的`normal`配置用于日常使用，`game4030`配置提高了两个风扇的最低转速（CPU和显卡风扇分别为40%,30%），适合游戏

#### 配置文件

* 软件同目录下的`config.json`
* 使用新的软件版本时**不支持**旧版配置文件自动升级，请手动编辑json文件
* 为防止关机时配置没有保存，系统托盘的每次变化都会触发配置文件保存

#### 界面（托盘的Config）

*建议手动编辑json，而不是使用界面，因为界面部分完成度较低*

* Profile: 可以选择编辑不同的配置方案，显示的内容为正在编辑的配置（不是正在使用的配置！只能在托盘选择要使用的配置）
* Temp-Speed模式：若温度处在温度列表（Temp栏）的两个值之间，则设置转速为较低温度对应的转速值（Speed栏），一般用于需要保证性能的场合
* Max Temp模式：若温度高于"Speed Up Temp"，则风扇转速百分比提高"Speed Step"，若低于"Slow Down Temp"，则降低"Speed Step"，最低不低于"Min Speed"，**说明：这是本软件推荐的方案，可以大幅降低噪声，但是会导致轻微降频（主要由于风扇提速不及时）；实际使用中发现会导致主板温度升高，建议在负载较高时把显卡风扇的最低转速设定得高一些，来带走主板热量**
* Static Speed：指定风扇转速百分比，主要用于调试风扇，勾选"enabled"来开启
* Speed Limit：限速功能，开启后Temp-Speed和Max Temp模式的转速都不会超过限制值
* 两个Interval值：检测核心温度并设定风扇转速的时间间隔，单位是毫秒。**注意：设定过低（即调速频率过高）可能会导致EC芯片出现问题**
* Clevo Auto：使用蓝天默认的“自动”模式
* Max Speed：全速模式（强冷）
* Apply按钮：应用设置并保存配置文件
* OK按钮：应用设置并保存配置文件，然后关闭界面

### Commands功能

* 此功能本身与风扇调速无关，但我发现对于使用Linux的蓝天电脑很有用
* 主要目的是Linux下一键以root身份执行命令，实现一键限频等功能
* 自带的`GPU-1200`用于限制显卡频率最高为1200MHz，`GPU-rgc`用于恢复默认显卡频率上限

### 显卡温度检测说明

* 目前没有发现比`nvidia-smi`更合适的检测方法，如果有想法请提[issue](https://github.com/elight2/ClevoFanControl/issues)；这种方法的最大缺点就是，`nvidia-smi`的运行会导致显卡被激活/打开，导致额外功耗，所以我采用了手动开关显卡温度检测的方式，使用显卡时在托盘里手动开启检测；关于自动检测显卡是否正在使用的功能说明如下
* 目前仅支持手动编辑配置文件
* 不支持Windows，理论上可以支持，有想法请提[issue](https://github.com/elight2/ClevoFanControl/issues)
* Linux下打开托盘的GPU温度检测开关后强制检测温度，无论自动检测是否启用
* Linux下的`config.json`中的`gpuAutoDetect`设置为`false`时关闭自动检测
* `gpuAutoDetect`为`true`时，根据`/sys`和`/dev`下的特定文件自动检测显卡是否正在使用，来决定是否使用`nvidia-smi`获取显卡温度，具体机制参考代码中的`GpuFanController::getTemp`
* 自动检测使用`lsof`检测打开显卡系统文件的进程，要排除的进程写在配置文件的`procExclude`数组中，需要根据本机情况修改

### 命令行版本(ClevoFanControl-cmd)
* 主要用于在没有gui的Linux系统上调节风扇，不依赖qt的lib文件,可以在极小的系统上运行
* 使用说明请使用`--help`或者`/?`参数查看
* **注意：输入的数据将直接发送给EC芯片，不会经过最大值最小值等安全检查，请保证输入正确**

### 开机自启动

#### Windows

1. 在”任务计划程序“中创建自定义任务
2. ”操作“选择启动`ClevoFanControl-gui`，起始目录好像需要设置为`ClevoFanControl-gui`所在目录
3. ”触发器“选择用户登录时
4. 勾选”使用最高权限运行“

#### Linux

1. 修改仓库里的[desktop文件](src/scripts/start-cfc.desktop)，确保目录正确
2. 把desktop文件添加至xdg autostart（一般为`~/.config/autostart`或者`/etc/xdg/autostart/`）
3. 在`/etc/sudoers`设置`ClevoFanControl-gui`无密码运行，即在`[用户名] ALL=(ALL:ALL)`后添加`NOPASSWD:[软件目录]/ClevoFanControl-gui`

## 构建
* 工具：cmake，ninja，gcc，qt6
1. 下载源码，cd到源码目录
2. 修改仓库中`src/scripts/`下的windows和linux的构建脚本，修改cmake配置选项，执行即可

## 原理

* [关于Windows环境的问题](https://tieba.baidu.com/p/9101786783)
* [Linux环境及原理解释](https://tieba.baidu.com/p/9101786783)
* 仓库里的`ec.zip`里面有我备份的参考资料

### 风扇控制接口

蓝天风扇调速原理请参考`ec.zip`里的资料

* Windows：[WinRing0](https://github.com/GermanAizek/WinRing0) [预编译文件](https://github.com/QCute/WinRing0)
* Linux：`inb()`和`outb()`

### 温度获取接口

|接口/系统|Windows                                   |Linux                            |
|--------|------------------------------------------|---------------------------------|
|CPU     |MSR寄存器中的`IA32_PACKAGE_THERM_STATUS_MSR`|`/sys/class/thermal/thermal_zone`|
|GPU     |`nvidia-smi`                              |`nvidia-smi`                      |
