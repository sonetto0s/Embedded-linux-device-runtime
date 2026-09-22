# module文件说明

## 当前架构所需函数及其作用说明

- main.c: 程序入口,负责检查基础标准FD、启动MiniShell并管理Shell生命周期

- shell.c: 控制Shell整体运行流程,负责初始化、select主循环、输入缓冲、Signal/Event处理以及资源释放

- shell_context.c: 管理Shell运行状态,统一保存JobManager、配置、稳定config路径、输入缓冲以及last_exit_status

- parser.c: 负责命令解析,将用户输入Tokenize后写入Command结构体以及Command链,并区分语法失败与内存失败状态

- command.c: 负责Command结构体初始化以及Command/argv/redirect相关资源释放

- dispatcher.c: 负责判断命令类型,查询Builtin Table,分发内部命令和外部命令,并处理Builtin重定向以及输出失败状态

- executor.c: 负责外部命令执行,实现fork、execvp、Process Group、startup gate、Pipeline、重定向、失败rollback以及Foreground Job等待

- builtin.c: 负责Shell内部命令实现,包括cd、pwd、exit、help、jobs、status、sysinfo、monitor、psinfo、dtinfo、hwinfo、led、fg、bg、reload等指令

- builtin_table.c: 负责Builtin指令集中注册以及查询,避免dispatcher内部大量if/else判断

- commands/cmd_runtime.c: 负责Runtime相关命令参数处理以及Console输出,当前实现monitor以及psinfo

- sig.c: 负责Signal初始化、事件记录、Shell退出时Signal关闭以及Child执行前默认Signal恢复

- job.c: 负责Job/Process状态管理,实现Job创建、Process加入、前后台等待、状态更新、回收以及Shell退出清理

- event.c: 负责self-pipe事件通知机制,将Signal事件传递至select主循环处理

- terminal.c: 负责控制TTY以及Foreground Process Group管理,保存/恢复Shell和Job的termios状态

- utils.c: 提供公共工具函数,辅助其他模块

- log.c: 负责日志系统,实现DEBUG、INFO、ERROR等日志输出

- error.c: 负责统一MiniShell错误码以及错误信息

- config.c: 负责配置初始化、配置解析、事务式配置加载以及reload支持

- system_info.c: 负责设备以及系统基础状态识别,读取Board、Kernel、CPU、Memory、Architecture、Uptime以及SoC Temperature等信息

- runtime_monitor.c: 负责读取Linux基础Runtime状态,包括CPU Usage、Memory Usage、Load Average、Process Count以及Uptime

- process_monitor.c: 负责读取Linux Process状态,支持单PID查询以及/proc Process列表扫描

- runtime_snapshot.c: 负责组合System、Thermal以及Network Runtime信息,并记录各Runtime Source状态

- network_monitor.c: 负责读取/sys/class/net中的Network Interface运行状态以及RX/TX Bytes

- thermal_monitor.c: 负责读取/sys/class/thermal中的Thermal Zone状态以及温度

- device_tree.c: 负责读取Linux Device Tree运行时信息,包括Model、Compatible以及Boot Args

- hardware_info.c: 负责读取运行时硬件状态,包括Thermal、CPUFreq、Network以及LED

- sysfs_io.c: 提供统一sysfs底层读取以及写入接口

- led_control.c: 负责Linux LED subsystem控制,完成Trigger以及Brightness修改并进行状态回读验证

## 主要数据结构

- ShellContext: 统一管理Shell运行状态

```
running
JobManager
last_exit_status
MiniShellConfig
config_file
input_buffer
input_length
input_discarding
```

- Command: 保存解析完成后的单条命令

```
argc
argv
background
redirect
next
```

- Command链: 用于表示Pipeline

```
Command
 |
Command
 |
Command
```

- JobManager: 统一管理当前Shell中的Job

```
head
nextid
```

- Job: 表示一个完整任务或者Pipeline

```
id
pgid
status
command
processes
terminal_modes
terminal_modes_valid
next
```

- Process: 表示Job内部的单个Direct Child

```
pid
status
wait_status
wait_status_valid
next
```

- MiniShellConfig: 保存当前Shell配置

```
prompts
max_job
debug
```

- RuntimeMonitor: 保存Linux基础运行状态

```
cpu_usage
memory_usage
load_average[3]
process_count
uptime
```

- ProcessInfo: 保存单个Linux Process状态

```
pid
name
state
rss_kb
threads
```

- NetworkRuntimeInfo: 保存单个Network Interface Runtime状态

```
name
state
rx_bytes
tx_bytes
has_rx
has_tx
```

- NetworkMonitor: 保存Network Interface集合

```
interfaces[]
count
```

- ThermalRuntimeInfo: 保存单个Thermal Zone Runtime状态

```
name
type
temperature
has_temperature
```

- ThermalMonitor: 保存Thermal Zone集合

```
zones[]
count
```

- RuntimeSnapshot: 保存当前Runtime状态

```
RuntimeMonitor runtime
ThermalMonitor thermal
NetworkMonitor network
available_sources
failed_sources
```

- SystemInfo: 保存当前系统以及板卡基础信息

```
board
kernel
hostname
architecture
cpu_model
cpu_cores
mem_total
mem_available
uptime
soc_temperature
has_soc_temperature
```

- DeviceTreeInfo: 保存Device Tree运行时信息

```
available
model
compatible
compatible_count
bootargs
```

- ThermalZoneInfo: 保存单个Thermal Zone信息

```
name
type
temperature
has_temperature
```

- CpuFreqPolicyInfo: 保存单个CPUFreq Policy信息

```
name
affected_cpus
driver
governor
current_khz
scaling_min_khz
scaling_max_khz
hardware_min_khz
hardware_max_khz
has_current
has_scaling_min
has_scaling_max
has_hardware_min
has_hardware_max
```

- NetworkInterfaceInfo: 保存单个Network Interface信息

```
name
state
address
mtu
has_mtu
```

- LedInfo: 保存单个LED运行状态

```
name
brightness
max_brightness
trigger
has_brightness
has_max_brightness
```

- HardwareInfo: 保存当前运行时硬件信息集合

```
thermal_zones
thermal_count
cpu_policies
cpu_policy_count
network_interfaces
network_count
leds
led_count
```

## Job状态

- JOB_RUNNING: Job中存在正在运行的Process

- JOB_STOPPED: Job中没有运行Process,但存在停止Process

- JOB_DONE: Job中所有已登记Process均已结束

## Process状态

- PROCESS_RUNNING: Process正在运行

- PROCESS_STOPPED: Process被停止

- PROCESS_DONE: Process已经结束并完成状态记录

## Shell相关函数

- shell_init: 初始化Log、ShellContext、Event、Terminal以及Signal

- shell_run: 进入select主循环,监听stdin和Event self-pipe

- shell_cleanup: 结束Job、关闭Signal/Event/Terminal并完成Shell退出

- shell_read_input: 单字节读取stdin并维护ShellContext输入状态

- shell_dispatch_input: 只在完整行形成后进入Parser/Dispatcher

## Parser相关函数

- tokenize: 将输入拆分为TokenList,识别Word、Pipe、Redirect、Background等Token

- build_command: 根据TokenList构建Command以及Pipeline Command链

- parse_line: 完成Tokenize以及Command构建流程

- new_command: 创建并初始化Command

Parser当前会明确区分:

```
空输入
语法错误 -> status 2
内存失败 -> status 1
```

## Dispatcher相关函数

- dispatcher_command: 查询Builtin Table并分发Builtin/External命令

- builtin_apply_redirect: 保存标准流并应用Builtin重定向

- builtin_restore_redirect: 恢复Shell原始标准流

Builtin执行完成后会检查`fflush(stdout)`结果,输出设备失败时即使Builtin逻辑本身成功也会返回非0状态.

## Executor相关函数

- execute_command: 根据Command链选择Single或者Pipeline执行

- execute_single: 执行单个外部命令

- execute_pipeline: 创建多级Pipeline并统一管理Process Group

- setredirect: 处理External输入/输出/追加重定向

- startup_gate_create: 创建Child启动同步pipe

- startup_gate_wait/startup_gate_release: 控制Child正式进入redirect/exec的时间点

- rollback_single/rollback_pipeline: startup失败时确定性清理未正式发布的Child

External exec失败状态:

```
ENOENT -> 127
其他无法执行错误 -> 126
```

## Signal/Event相关函数

- signal_init: 初始化Shell Signal处理策略

- signal_shutdown: Shell退出时停止SIGINT/SIGCHLD通知路径

- signal_take_events: 读取并清除当前pending Signal事件

- signal_reset_child: Child进入redirect/exec前恢复默认Signal行为以及Signal mask

- event_init: 创建nonblocking/CLOEXEC self-pipe

- event_getfd: 获取self-pipe读端

- event_notify: 向Event pipe写入事件通知

- event_drain: 清空Event pipe已有数据

- event_close_in_child: Child关闭Shell内部Event FD

- event_shut: Shell退出时关闭Event FD

## Job相关函数

- jobmanager_init: 初始化JobManager

- job_add: 新建Job并加入JobManager

- process_add: 向Job加入MiniShell直接Child

- job_find: 根据pgid查找Job

- job_list: 输出当前Job状态

- job_wait_foreground: 等待整个Foreground Process Group状态变化

- job_continue: 向停止Job发送SIGCONT并更新状态

- job_exit_status: 获取Job最后一个Process退出状态

- job_count_active: 统计当前活动Job数量

- job_reap: 使用waitpid异步回收Child状态

- job_cleanup_done: 清理已经完成的Job

- job_remove: 删除指定Job

- job_shutdown: Shell退出时先TERM Process Group,有限等待后KILL Process Group并回收Direct Child

- job_destroy: 释放JobManager中的全部Job/Process数据

Job Shutdown的Signal作用范围是Process Group,但waitpid仍只负责MiniShell直接拥有的Child.

## Terminal相关函数

- terminal_init: 打开`/dev/tty`,初始化Shell Process Group并保存Shell termios

- terminal_shutdown: Shell退出时恢复并关闭控制TTY

- terminal_is_initialized: 判断当前是否拥有可用TTY环境

- terminal_set_foreground: 将指定Process Group设置为Terminal前台

- terminal_get_modes: 读取当前Terminal Modes

- terminal_set_modes: 设置Terminal Modes

- terminal_restore: 将Terminal前台以及termios恢复至Shell

Job结构体会保存STOPPED状态下的termios,fg继续运行前恢复原Job Terminal Modes.

## Config相关函数

- config_init: 设置默认配置

- config_parse_line: 解析单行配置并返回明确状态

- config_load: 加载到temporary config,完整成功后再替换当前配置

当前支持:

```
prompts
max_job
debug
```

当前Config查找顺序:

```
MINISHELL_CONFIG
程序部署目录
开发目录
当前工作目录
内部默认配置
```

ShellContext初始化时会保存最终Config路径.

启动后:

```
cd
```

不会改变:

```
reload
```

使用的Config来源.

## System Info相关函数

- system_info_collect: 收集当前系统以及板卡基础状态

- system_info_print: 输出SystemInfo

当前读取:

```
Board
Kernel
Hostname
Architecture
CPU Model
CPU Cores
SoC Temperature
Memory Total
Memory Available
Uptime
```

Orange Pi Board信息主要来自:

```
/proc/device-tree/model
```

CPU Core通过:

```
sysconf(_SC_NPROCESSORS_ONLN)
```

读取.

SoC Temperature通过Thermal Zone中的:

```
soc-thermal
```

获取.

## Runtime Command相关函数

- cmd_monitor: 检查monitor参数,调用RuntimeSnapshot并输出当前Runtime状态

- cmd_psinfo: 检查psinfo参数,调用ProcessMonitor并输出单个或者全部Process状态

文件:

```
include/cmd_runtime.h
src/commands/cmd_runtime.c
```

Command Layer只负责参数以及输出.

底层Runtime模块不直接依赖ShellContext或者Builtin.

## RuntimeMonitor相关函数

- runtime_monitor_collect: 收集CPU、Memory、Load、Process Count以及Uptime

读取:

```
/proc/stat
/proc/meminfo
/proc/loadavg
/proc/uptime
/proc
```

CPU Usage使用两次Snapshot差分计算.

采样间隔:

```
100ms
```

## ProcessMonitor相关函数

- process_monitor_get: 读取指定PID的ProcessInfo

- process_monitor_collect: 扫描/proc并生成ProcessInfo Array

- process_monitor_free: 释放ProcessInfo Array

读取:

```
/proc/<pid>/status
```

字段:

```
Name
State
VmRSS
Threads
```

扫描期间已经退出的PID直接跳过.

最终列表按照PID升序排列.

## RuntimeSnapshot相关函数

- runtime_snapshot_collect: 收集System、Thermal以及Network Runtime状态

- runtime_snapshot_hottest_thermal: 获取当前最高有效Temperature的Thermal Zone

- runtime_snapshot_primary_network: 获取当前Primary Network Interface

当前Source:

```
RUNTIME_SNAPSHOT_SOURCE_SYSTEM
RUNTIME_SNAPSHOT_SOURCE_THERMAL
RUNTIME_SNAPSHOT_SOURCE_NETWORK
```

System失败时Snapshot失败.

Thermal或者Network失败时记录failed_sources,其他Runtime信息继续保留.

## NetworkMonitor相关函数

- network_monitor_collect: 从/sys/class/net读取Network Interface状态

- network_monitor_collect_from: 从指定Directory读取Network Interface状态,主要用于Test Fixture

- network_monitor_primary: 选择Primary Interface

当前读取:

```
operstate
statistics/rx_bytes
statistics/tx_bytes
```

Primary选择:

```
非lo + up
 |
第一个非lo
 |
第一个Interface
```

## ThermalMonitor相关函数

- thermal_monitor_collect: 从/sys/class/thermal读取Thermal Zone

- thermal_monitor_collect_from: 从指定Directory读取Thermal Zone,主要用于Test Fixture

- thermal_monitor_hottest: 选择当前最高有效Temperature

当前读取:

```
thermal_zone*/type
thermal_zone*/temp
```

不会固定依赖thermal_zone0.

## Device Tree相关函数

- device_tree_collect: 收集Device Tree Runtime信息

- device_tree_print: 输出DeviceTreeInfo

主要读取:

```
/proc/device-tree/model
/proc/device-tree/compatible
/proc/device-tree/chosen/bootargs
```

Compatible内容属于NUL分隔字符串集合,模块会逐项解析.

不存在Device Tree接口时:

```
available = 0
```

不会作为Shell致命错误处理.

## Hardware Info相关函数

- hardware_info_collect: 收集全部运行时硬件状态

- hardware_info_print: 输出HardwareInfo

当前收集:

```
Thermal
CPUFreq
Network
LED
```

对应Linux接口:

```
/sys/class/thermal
/sys/devices/system/cpu/cpufreq
/sys/class/net
/sys/class/leds
```

hardware_info负责解释硬件信息.

底层sysfs访问统一由:

```
sysfs_io
```

完成.

## Sysfs相关函数

- sysfs_read_text: 读取sysfs文本属性并去除末尾换行

- sysfs_read_long: 读取有符号数字属性

- sysfs_read_ulong: 读取无符号数字属性

- sysfs_write_text: 写入sysfs文本属性

sysfs文件使用:

```
O_CLOEXEC
```

避免内部FD被Child exec继承.

read/write处理:

```
EINTR
```

数字解析会检查完整字符串是否合法.

## LED Control相关函数

- led_control_set_on: 将指定LED切换到手动模式并设置最大Brightness

- led_control_set_off: 将指定LED切换到手动模式并将Brightness设置为0

- led_control_set_trigger: 设置指定LED Trigger

on流程:

```
检查LED Name
 |
读取max_brightness
 |
trigger=none
 |
重新读取trigger
 |
写入max_brightness
 |
重新读取brightness
 |
完成
```

off流程:

```
检查LED Name
 |
trigger=none
 |
重新读取trigger
 |
brightness=0
 |
重新读取brightness
 |
完成
```

Trigger流程:

```
检查LED Name以及Trigger
 |
写入trigger
 |
重新读取Active Trigger
 |
验证结果
```

LED控制不会绕过Linux权限机制.

## Builtin相关功能

当前Builtin Table:

```
cd
pwd
exit
jobs
help
status
sysinfo
monitor
psinfo
dtinfo
hwinfo
led
fg
bg
reload
```

Runtime相关Builtin:

```
monitor
psinfo
```

实际处理通过:

```
cmd_runtime
```

Hardware相关Builtin:

```
dtinfo
hwinfo
led
```

led支持:

```
led list
led info <name>
led on <name>
led off <name>
led trigger <name> <trigger>
```

`led list`以及`led info`复用HardwareInfo读取结果.

写操作通过:

```
led_control
```

完成.

## 外部命令退出状态

当前采用:

```
0        成功
1        普通/内部/重定向失败
2        Parser语法错误
126      找到目标但无法执行
127      命令未找到
128+sig  Signal结束
```

Pipeline使用最后一个Process的退出状态.

## 资源生命周期

Command:

```
Command创建
 |
Parser填充argv/redirect
 |
Dispatcher/Executor使用
 |
command_free
```

Job:

```
Job创建
 |
job_add
 |
process_add
 |
Process运行/停止/结束
 |
job_reap
 |
job_remove/job_cleanup_done
 |
释放Job/Process
```

Shell:

```
Shell退出
 |
job_shutdown
 |
signal_shutdown
 |
event_shut
 |
terminal_shutdown
 |
shell_context_destroy
```

Sysfs:

```
open
 |
read/write
 |
close
```

## 测试文件

- test_main.c: Unit Test程序入口

- test_framework.c/test_framework.h: 自定义轻量测试框架

- test_config.c: 测试Config解析、加载、非法配置以及事务式更新

- test_parser.c: 测试Parser、Pipeline、Redirect、Token边界以及错误状态

- test_log.c: 测试Log模块

- test_event.c: 测试Event生命周期、FD属性、Notify Flood以及Shutdown安全

- test_shell_context.c: 测试ShellContext初始化、稳定Config路径以及销毁

- test_builtin_table.c: 测试Builtin Table查询以及当前Builtin注册

- test_system_info.c: 测试System Info读取以及重复覆盖

- test_runtime_monitor.c: 测试CPU、Memory、Load、Process Count以及Uptime

- test_process_monitor.c: 测试单PID读取、/proc扫描以及PID排序

- test_network_monitor.c: 使用/tmp Fixture测试Network Interface状态、RX/TX Bytes以及Primary Interface

- test_thermal_monitor.c: 使用/tmp Fixture测试Thermal Zone读取以及最高温度选择

- test_runtime_snapshot.c: 测试Runtime Source状态以及RuntimeSnapshot读取

- test_device_tree.c: 测试Device Tree接口以及无Device Tree平台行为

- test_hardware_info.c: 测试Hardware Info收集以及数量边界

- test_sysfs_io.c: 测试公共sysfs I/O参数、文本以及数字处理

- test_led_control.c: 测试LED Name以及Trigger非法输入

- test_dispatcher.c: 测试Builtin/External分发以及Builtin Redirect

- test_executor.c: 测试Single、Pipeline、Child回收以及Job上限

- test_job.c: 测试JobManager、Job、Process、Shutdown以及同组后代清理

- test_job_control.c: 测试Signal/Event/Foreground Job Control相关逻辑

- test_command.c: 测试Command初始化以及资源释放

## Integration测试文件

- test_integration_main.c: Integration Test入口

- test_shell_runner.c: 启动真实MiniShell,使用pipe/poll/monotonic deadline运行脚本并收集输出

- test_shell_basic.c: 测试基础命令以及monitor/psinfo Runtime Command

- test_shell_redirect.c: 测试输入/输出/追加重定向

- test_shell_pipeline.c: 测试多级Pipeline以及返回状态

- test_shell_status.c: 测试Shell退出状态、126/127、Builtin输出失败、reload以及高FD行为

- test_shell_background.c: 测试后台任务以及SIGCHLD回收

- test_shell_input.c: 测试分段输入与SIGCHLD同时发生时的输入生命周期

- test_shell_pty.c: 使用PTY测试Ctrl+C、Ctrl+Z、fg、bg、Terminal Job Control、FIFO Signal、termios以及后台启动规则

## Stability测试文件

- arm_runtime_stability.sh: 在Orange Pi中持续运行同一个MiniShell进程,进行Foreground、Pipeline、Redirect、reload、Background、Hardware Info以及Runtime Manager压力验证

主要检查:

```
FD
RSS
Zombie
Shell存活
Shell退出
Config reload
Hardware Runtime
Runtime Monitor
Process Monitor
```

## 当前测试状态

Orange Pi 5 Plus当前验证:

```
Unit Test:

111 Cases
0 Failed


Integration Test:

46 Cases
0 Failed
```

ProcessMonitor测试会根据当前系统Process数量执行部分排序Assertion,因此Assertion数量可能随运行环境变化.
