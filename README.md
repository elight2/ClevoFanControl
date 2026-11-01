# 蓝天电脑风扇自动调速工具

> **此软件会向EC芯片发送命令，有可能造成电脑损坏，作者对本软件造成的损坏概不负责！**

> 目前v3版本仅支持Linux，v2版本有Windows支持

> v3版本进行了完全重构，代码大部分由Agent完成

## 开源Git仓库
* [ClevoFanControl](https://github.com/elight2/ClevoFanControl)

## 配置文件
[config.toml](./src/config.toml)
> 文件里有注释说明每个参数
> 默认配置是根据作者电脑情况写的，**建议根据自己的机器散热情况修改配置，不要使用默认版本**

## 安装
> 建议通过systemd管理
* 复制程序和配置文件到`/root/`下某个目录
* 编写systemd单元设置自启动

## 调速机制

### 调速线程
此线程有2个，分别控制2个风扇，**线程仅负责调速，不负责读取数据（温度数据以及调速参数），以此实现了2种调速模式，后面有解释**

每过`ctrl_interval`执行一次调速操作：
1. 检查当前风扇速度是否低于**当前的**最低允许速度，如果低于则将速度提升`ctrl_step`，并跳过本次循环
2. 如果核心温度低于`target_temp - target_temp_offset`，则将速度降低`ctrl_step`，如果高于`target_temp + target_temp_offset`，则提升`ctrl_step`，如果处于区间内则不操作
3. 任何情况下风扇速度不允许超过`max_speed`（如果提升最小`ctrl_step`后超过了则限制在`max_speed`以下）

### 软件控制线程
每过`hw_detect_interval`执行一次操作：
1. 记录当前的核心功耗，并且保留包括本次在内的`power_sample_count`个数据，计算他们的平均值作为平均功率
2. 根据`min_speed_list`的对照关系，设置当前的最低允许速度（e.g. 对照表里是20W-20%, 30W-30%，则在20W-29W平均功率时，取20%最低允许速度；如果平均功率低于最小设置则使用最小设置对应的速度，超过最大设置则使用最大设置对应的速度）

此线程同时负责显卡状态自动检测和模式动态切换：
* 独显状态判断：通过sys文件系统检测独显当前处于`active`还是`suspended`状态，并据此分别启用GPU+CPU，单CPU调速模式
> 获取独显功耗和温度时会唤醒独显，导致功耗大幅增加，所以需要区分2种模式，避免在独显不使用时对其进行无用唤醒
* GPU+CPU模式：功耗，温度数据来源分别来自GPU和CPU的真实数据
* 单CPU模式：使用CPU的功耗，温度替代GPU风扇调速线程使用的调速数据
* 独显睡眠功能（可以使用`gpu_sleep_detect_enabled`关闭）：当目前处于GPU+CPU模式时，如果检测到使用独显的进程数为0，则会暂停检测独显功耗和温度`gpu_sleep_delay`秒，以让独显有机会自动睡眠，而`gpu_proc_exclude`里的进程不会被计入；此判断仅在独显温度低于`gpu_sleep_temp`时开始，暂停检测期间的功耗和温度数据将沿用最后一次检测的数据，**如果暂停区间过后再次判断到没有进程使用独显，判断将继续（检测停止），直到独显睡眠，如果出现问题请关闭此功能**

## EC芯片及其协议
**本部分仅在蓝天n960kp型号验证**
* 芯片型号：it5570e-128
* 所有参考资料放在[ec-reference.zip](./ec-reference.zip)
* 部分通信协议细节以及封装好的函数库在[libclevoec.c](./src/libclevoec.c)

## 风扇调试脚本（Python）
* [testfan.py](./src/testfan.py)
* **不要同时使用调试脚本和软件本体**

## 构建
* Release模式：`cmake -B build -DCMAKE_BUILD_TYPE=Release -S . -G Ninja && cmake --build build && cmake --install build`
* 输出在`out/`

