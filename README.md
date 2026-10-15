# 蓝天电脑风扇自动调速工具

> **此软件会向EC芯片发送命令，有可能造成电脑损坏，作者对本软件造成的损坏不负责！**

> 最新版目前仅支持Linux，旧版有Windows支持

## 调速机制

### 参数
以下参数都能在配置文件`config.toml`修改：
* `min_speed_list`: 2维数组
* `power_sample_count`: 整数
* `target_temp`: 整数
* `target_temp_offset`: 整数
* `ctrl_step`: 整数
* `ctrl_interval`: 浮点数（秒）

**以上参数有3份，分别对应CPU风扇参数，GPU风扇参数，GPU风扇参数（单CPU模式），他们分别存放在`[cpu] [gpu] [gpu_cpu_only]`3个表，后面有解释**

下面的参数在`[general]`表，是全局设置

* `log_level`: 字符串，输出日志等级（normal, debug）
* `hw_detect_interval`: 浮点数（秒）

### 自动调速原理

#### 调速线程
此线程有2个，分别控制2个风扇，但**调速线程不知道自己使用的数据（温度数据以及调速参数）来源于GPU还是CPU**，后面有解释

每过`ctrl_interval`执行一次调速操作：
1. 检查当前风扇速度是否低于**当前的**最低允许速度，如果低于则将速度提升`ctrl_step`，并跳过本次循环
2. 如果核心温度低于`target_temp - target_temp_offset`，则将速度降低`ctrl_step`，如果高于`target_temp + target_temp_offset`，则提升`ctrl_step`，如果处于区间内则不操作

#### 软件控制线程
每过`hw_detect_interval`执行一次操作：
1. 记录当前的核心功耗，并且保留包括本次在内的`power_sample_count`个数据，计算他们的平均值作为平均功率
2. 根据`min_speed_list`的对照关系，设置当前的最低允许速度（e.g. 对照表里是20W-20%, 30W-30%，则在20W-29W平均功率时，取20%最低允许速度；如果平均功率低于最小设置则使用最小设置对应的速度，超过最大设置则使用最大设置对应的速度）

此线程同时负责显卡状态自动检测和模式动态切换：
* 通过sys文件系统检测独显当前处于`active`还是`suspended`状态，并据此分别启用GPU+CPU，单CPU调速模式
* GPU+CPU模式：功耗，温度数据来源分别来自GPU和CPU的真实数据
* 单CPU模式：使用CPU的功耗，温度替代GPU风扇调速线程使用的调速数据
> 获取独显功耗和温度时会唤醒独显，导致功耗大幅增加，所以需要区分2种模式，避免在独显不使用时对其进行无用唤醒

## EC芯片及其协议
**本部分仅在蓝天n960kp型号验证**
* 芯片型号：it5570e-128
TODO

