# architecture文件说明

## 本文件用以记录此项目整体架构实现,模块职责以及程序运行流程

## 整体架构

```
                         main
                          |
                     shell_init
                          |
                shell_context_init
                          |
                     event_init
                          |
                   terminal_init
                          |
                    signal_init
                          |
                     shell_run
                          |
                select监听stdin/Event
                          |
              |                           |
           stdin                       event
              |                           |
        read 1 byte                 event_drain
              |                           |
       input_buffer              signal_take_events
              |                           |
       遇到换行?              |                     |
          |                 SIGCHLD                SIGINT
        parse_line              |                     |
          |                  job_reap            清理输入状态
       tokenize                 |                 重新输出Prompt
          |
    build_command
          |
      Command链
          |
 dispatcher_command
          |
     |                    |
  Builtin              Executor
     |                    |
     |               execute_command
     |                    |
     |           |------------------|
     |      execute_single     execute_pipeline
     |           |                  |
     |         fork()           pipe()/fork()
     |           |                  |
     |      startup gate        startup gate
     |           |                  |
     |      Process Group       Process Group
     |           |                  |
     |         Job登记            Job登记
     |           |                  |
     |      Terminal handoff    Terminal handoff
     |           |                  |
     |        release GO          release GO
     |                              |
     |------------------------------|
                    |
              ShellContext状态
```

Shell Core之外当前包含Hardware Runtime以及Runtime Monitor相关模块:

```
                    Builtin
                       |
       |---------------|------------------------|
       |               |                        |
    sysinfo          dtinfo               Runtime Command
    hwinfo             |                        |
       |            device_tree            cmd_runtime
       |                                        |
       |                             |----------|----------|
       |                           monitor               psinfo
       |                             |                    |
hardware_info                 RuntimeSnapshot      ProcessMonitor
system_info                         |
       |                    |-------|-------|
   sysfs_io             Runtime  Thermal  Network
       |                Monitor  Monitor  Monitor
      /sys                 |       |        |
                           |      /sys     /sys
                          /proc
```

LED控制:

```
led
 |
led_control
 |
sysfs_io
 |
/sys/class/leds
 |
Linux LED subsystem
 |
Orange Pi
```

## 输入流程

MiniShell不使用一次getline调用代表一整条命令.

```
select
 |
stdin readable
 |
read 1 byte
 |
写入ctx->input_buffer
 |
 |--------------------|
 |                    |
不是换行              遇到换行
 |                    |
继续select         shell_dispatch_input
                       |
                   parse_line
                       |
                  dispatcher
```

这样做主要解决两个问题:

```
SIGCHLD等事件打断输入
不会把半条命令提前执行

Shell不会一次预读后续stdin数据
避免吃掉Foreground Child应该读取的数据
```

当输入超过:

```
SHELL_INPUT_SIZE
```

时,Shell进入discard状态.

当前超长行会被丢弃直到下一次换行.

Ctrl+C发生在Prompt输入阶段时:

```
SIGINT
 |
Event唤醒
 |
shell_reset_input
 |
丢弃当前未完成输入
 |
重新输出Prompt
```

## Parser流程

```
用户输入
 |
tokenize
 |
TokenList
 |
build_command
 |
Command / Command链
```

Parser识别:

```
Word
Pipe
Input Redirect
Output Redirect
Append Redirect
Background
```

Parser返回状态区分:

```
空输入
语法错误
内存失败
```

## Dispatcher流程

```
Command
 |
builtin_lookup
 |
 |----------------------|
 |                      |
Builtin               External
 |                      |
Builtin Redirect      execute_command
 |                      |
Builtin Handler       Executor
```

Builtin执行以后会检查:

```
fflush(stdout)
```

因此即使Builtin内部逻辑成功,输出设备最终写入失败时仍然返回非0状态.

## Runtime Command流程

Runtime相关Builtin不会直接在builtin.c中读取/proc或者/sys.

```
Builtin Table
 |
builtin_monitor / builtin_psinfo
 |
cmd_runtime
 |
 |-----------------------|
 |                       |
monitor                 psinfo
 |                       |
RuntimeSnapshot       ProcessMonitor
```

cmd_runtime负责:

```
参数检查
Runtime API调用
Console输出
错误输出
```

依赖方向:

```
Shell/Builtin
 |
cmd_runtime
 |
Runtime Core
```

Runtime Core不依赖:

```
builtin
shell_context
cmd_runtime
```

## RuntimeMonitor流程

```
runtime_monitor_collect
 |
 |-------------------------------|
 |        |        |        |    |
CPU     Memory    Load    Uptime Process
```

当前保存:

```
cpu_usage
memory_usage
load_average[3]
process_count
uptime
```

CPU:

```
/proc/stat
 |
Snapshot 1
 |
100ms
 |
Snapshot 2
 |
total_delta / idle_delta
 |
CPU Usage
```

Memory:

```
/proc/meminfo
 |
MemTotal
MemAvailable
```

Load:

```
/proc/loadavg
```

Uptime:

```
/proc/uptime
```

Process Count:

```
/proc
 |
数字PID目录
 |
count
```

## ProcessMonitor流程

单个PID:

```
psinfo <pid>
 |
process_monitor_get
 |
/proc/<pid>/status
 |
ProcessInfo
```

当前读取:

```
Name
State
VmRSS
Threads
```

全部Process:

```
psinfo
 |
process_monitor_collect
 |
/proc
 |
数字PID
 |
process_monitor_get
 |
ProcessInfo[]
 |
qsort
 |
PID升序
```

Process在扫描期间可能退出.

```
发现PID
 |
读取status失败
 |
skip
```

不会因为一个PID消失导致整个Process列表失败.

## RuntimeSnapshot流程

monitor使用RuntimeSnapshot统一收集状态.

```
monitor
 |
cmd_monitor
 |
runtime_snapshot_collect
 |
 |-------------------------|
 |            |            |
Runtime    Thermal       Network
Monitor    Monitor       Monitor
 |            |            |
/proc       /sys          /sys
 |
RuntimeSnapshot
 |
print_runtime_snapshot
```

RuntimeSnapshot保存:

```
RuntimeMonitor runtime
ThermalMonitor thermal
NetworkMonitor network
available_sources
failed_sources
```

System Runtime读取失败时Snapshot失败.

Thermal或者Network读取失败时:

```
failed_sources记录
 |
其他Runtime数据继续使用
```

## ThermalMonitor流程

```
thermal_monitor_collect
 |
/sys/class/thermal
 |
thermal_zone*
 |
 |-----------|
 |           |
type        temp
 |
ThermalRuntimeInfo[]
```

当前通过:

```
thermal_monitor_hottest
```

选择存在有效Temperature的最高温度Zone.

不会固定使用thermal_zone0.

`thermal_monitor_collect_from`允许Test使用临时Directory模拟Thermal接口.

## NetworkMonitor流程

```
network_monitor_collect
 |
/sys/class/net
 |
Interface
 |
 |-----------------------------|
 |              |              |
operstate    rx_bytes        tx_bytes
 |
NetworkRuntimeInfo[]
```

Primary Interface选择:

```
非lo + up
 |
第一个非lo
 |
第一个现有Interface
```

`network_monitor_collect_from`允许Test使用临时Directory模拟Network接口.

## Child启动流程

Single以及Pipeline都使用startup gate.

```
fork
 |
 |--------------------------------|
 |                                |
Child                           Parent
 |                                |
Block startup signals            setpgid
 |                                |
setpgid                           job_add
 |                                |
关闭Shell Event FD              process_add
 |                                |
等待startup GO             terminal handoff
 |                                |
 |<------------- GO --------------|
 |
signal_reset_child
 |
redirect
 |
execvp
```

该流程保证Foreground Child不会在Parent完成:

```
Process Group
Job登记
Process登记
Terminal交接
```

以前进入用户程序.

Child等待startup gate期间暂时Block交互相关Signal.

收到GO以后恢复默认Signal disposition以及Signal mask,然后进入redirect/exec.

如果Parent端发生Job/Process登记失败等问题,未正式发布的Child会进入rollback路径进行确定性清理.

## Pipeline流程

```
Command链
 |
逐个创建pipe/fork
 |
全部Child进入同一pgid
 |
Child等待startup gate
 |
Parent完成Job/Process登记
 |
Foreground完成Terminal交接
 |
释放全部Child
 |
Child建立pipe stdin/stdout
 |
处理redirect
 |
execvp
 |
Parent等待整个Process Group
```

Pipeline退出状态使用最后一个Process的状态.

当前不实现:

```
pipefail
```

## 后台任务流程

```
Command &
 |
execute_command
 |
fork/process group
 |
job_add
 |
process_add
 |
startup gate release
 |
Shell立即返回Prompt
 |
SIGCHLD
 |
event_notify
 |
select监听Event
 |
event_drain
 |
signal_take_events
 |
job_reap
 |
更新Process/Job状态
 |
job_cleanup_done
```

## 前台Job流程

```
Command
 |
fork/process group
 |
startup gate
 |
job_add/process_add
 |
terminal_set_foreground
 |
release child
 |
job_wait_foreground
 |
waitpid(-pgid)
 |
更新Process状态
 |
更新Job状态
 |
如果STOPPED则保存Job termios
 |
terminal_restore
 |
Shell重新获得Terminal以及Shell termios
```

## Ctrl+Z流程

```
Foreground Job
 |
Ctrl+Z
 |
SIGTSTP发送给Foreground Process Group
 |
Process停止
 |
waitpid返回WIFSTOPPED
 |
PROCESS_STOPPED
 |
JOB_STOPPED
 |
保存Job Terminal Modes
 |
terminal_restore
 |
jobs可以查看状态
```

## fg流程

```
STOPPED/RUNNING Job
 |
terminal_set_foreground
 |
恢复Job Terminal Modes
 |
如为STOPPED则job_continue
 |
SIGCONT
 |
job_wait_foreground
 |
Job结束/再次停止
 |
保存新的Job Terminal Modes
 |
terminal_restore
```

控制TTY由terminal模块单独打开:

```
/dev/tty
```

因此不依赖:

```
STDIN_FILENO
```

fd 0被Builtin重定向以后仍然可以正常完成Foreground Process Group切换.

## bg流程

```
STOPPED Job
 |
job_continue
 |
SIGCONT
 |
PROCESS_RUNNING
 |
JOB_RUNNING
 |
Shell继续运行
```

## Signal/Event流程

```
SIGCHLD/SIGINT
 |
Signal Handler
 |
设置pending_events
 |
event_notify
 |
self-pipe写入
 |
select唤醒
 |
event_drain
 |
signal_take_events
 |
Shell正常上下文处理事件
```

Signal Handler只完成轻量事件记录以及self-pipe通知.

不会直接调用:

```
waitpid
malloc
printf
复杂Job操作
```

Shell退出时:

```
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

避免Signal Handler仍然使用Event pipe时另一边已经关闭FD.

## Job Shutdown流程

```
Shell退出
 |
STOPPED Job先SIGCONT
 |
对Job pgid发送SIGTERM
 |
有限次数尝试回收
 |
仍存在Process Group发送SIGKILL
 |
waitpid回收MiniShell Direct Child
 |
job_destroy
```

Signal范围以Process Group为单位.

waitpid只负责MiniShell实际拥有的Direct Child.

即使登记的Direct Child已经结束,最终仍会尝试清理原Process Group中可能残留的后代Process.

## Builtin重定向流程

```
Builtin Command
 |
保存原stdin/stdout
 |
打开重定向文件
 |
dup2切换标准流
 |
执行Builtin
 |
fflush检查真实输出结果
 |
恢复原stdin/stdout
 |
clearerr
 |
返回命令状态
```

如果Builtin逻辑本身成功,但最终输出flush失败,命令仍返回非0状态.

如果Shell无法恢复自身标准流,视为Shell健康状态失败.

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

Pipeline使用最后一个Process退出状态.

## Config流程

当前Config路径解析:

```
Shell启动
 |
检查MINISHELL_CONFIG
 |
获取/proc/self/exe
 |
解析Executable Directory
 |
解析启动cwd
 |
按照优先级查找Config
 |
保存稳定config_file
 |
config_load
```

查找顺序:

```
MINISHELL_CONFIG
 |
<exe>/../config/config.conf
 |
<exe>/config/config.conf
 |
<cwd>/config/config.conf
 |
内部默认配置
```

显式设置:

```
MINISHELL_CONFIG
```

时优先使用该文件.

指定文件失败不会静默回退到其他Config.

相对路径在启动时转换成稳定路径.

因此:

```
cd /tmp
reload
```

仍然使用启动时确定的Config来源.

## System Info流程

```
sysinfo
 |
system_info_collect
 |
 |-----------------------------|
 |                             |
Linux基础信息                ARM板卡信息
 |                             |
uname                         Device Tree model
/proc/meminfo                 sysconf CPU cores
/proc/uptime                  Thermal soc-thermal
 |
SystemInfo
 |
system_info_print
```

当前输出:

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

可选板卡接口不存在时输出:

```
N/A
```

不会使Shell退出.

## Device Tree流程

```
dtinfo
 |
device_tree_collect
 |
/proc/device-tree
 |
 |-------------------------|
 |            |            |
model     compatible     chosen/bootargs
 |            |            |
 |------- DeviceTreeInfo ---|
 |
device_tree_print
```

Compatible文件内容是:

```
NUL separated strings
```

模块逐项解析并保存.

在不存在Device Tree的平台:

```
available = 0
```

调用仍然正常返回.

## Hardware Info流程

```
hwinfo
 |
hardware_info_collect
 |
 |--------------------------------|
 |            |          |        |
Thermal     CPUFreq    Network    LED
 |            |          |        |
 |------------ sysfs_io -----------|
               |
              /sys
               |
          Linux Kernel
```

对应接口:

```
/sys/class/thermal
/sys/devices/system/cpu/cpufreq
/sys/class/net
/sys/class/leds
```

hardware_info负责解释硬件信息.

底层文件访问由sysfs_io统一完成.

## Sysfs流程

文本读取:

```
sysfs_read_text
 |
open(O_RDONLY | O_CLOEXEC)
 |
read
 |
EINTR处理
 |
close
 |
去除末尾换行
```

数字读取:

```
sysfs_read_text
 |
strtol/strtoul/strtoull
 |
检查完整字符串
 |
输出数字
```

写入:

```
sysfs_write_text
 |
open(O_WRONLY | O_CLOEXEC)
 |
write
 |
EINTR处理
 |
close
```

内部sysfs FD使用:

```
O_CLOEXEC
```

避免被Child exec继承.

## LED读取流程

```
led list / led info
 |
hardware_info_collect
 |
/sys/class/leds
 |
brightness
max_brightness
trigger
```

## LED控制流程

打开LED:

```
led on <name>
 |
检查Name
 |
读取max_brightness
 |
trigger=none
 |
回读Active Trigger
 |
brightness=max
 |
回读Brightness
 |
返回结果
```

关闭LED:

```
led off <name>
 |
检查Name
 |
trigger=none
 |
回读Active Trigger
 |
brightness=0
 |
回读Brightness
 |
返回结果
```

设置Trigger:

```
led trigger <name> <trigger>
 |
检查Name以及Trigger
 |
写入trigger
 |
读取Active Trigger
 |
验证结果
```

MiniShell不会自动修改sysfs权限.

权限仍由Linux控制.

## Error处理流程

```
发生错误的底层模块
 |
掌握errno/path/pid等上下文
 |
输出具体诊断
 |
返回MiniShell错误码/Unix状态
 |
上层负责状态归一以及生命周期决策
```

避免上层在已经输出具体错误以后再重复输出无意义错误.

## 资源释放流程

Command:

```
Command创建
 |
Parser填充argv/redirect
 |
Dispatcher/Executor使用
 |
command_free释放
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
job_reap更新状态
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
结束Process Group
 |
回收Direct Child
 |
job_destroy
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

## 部署流程

默认部署结构:

```
/opt/minishell/
├── bin/
│   └── minishell
└── config/
    └── config.conf
```

Make部署:

```
Source
 |
make
 |
build/default/minishell
 |
make install
 |
/opt/minishell
```

临时rootfs:

```
Source
 |
make install DESTDIR=/tmp/minishell-root
 |
/tmp/minishell-root/opt/minishell
 |
目标rootfs
```

CMake保持相同最终目录结构.

详细部署流程:

```
docs/deployment.md
```

## ARM Runtime Stability流程

```
普通Build
 |
启动一个持续运行MiniShell
 |
连续执行大量命令
 |
Foreground
Pipeline
Redirect
reload
Background
Hardware Info
 |
检查/proc/<pid>/fd
 |
检查/proc/<pid>/status
 |
检查Zombie
 |
检查Shell存活
 |
exit
 |
检查正常退出
```

稳定性测试不会替代ASan以及Integration Test.

三者关注点分别为:

```
Integration -> 功能组合正确性
Sanitizer   -> 内存以及Undefined Behavior
Stability   -> 长时间资源以及运行状态
```

## 当前整体分层

```
用户输入
 |
Shell Core
 |
 |--------------------------|---------------------------|
 |                          |                           |
Command/Process         Runtime Manager            Hardware Runtime
 |                          |                           |
Parser                  RuntimeMonitor              System Info
Dispatcher              ProcessMonitor              Device Tree
Executor                 RuntimeSnapshot            Hardware Info
Job                      ThermalMonitor             LED Control
Signal                    NetworkMonitor                 |
Event                         |                      Sysfs IO
Terminal                       |                           |
 |                            /proc /sys             Linux Kernel
 |                                                        |
 |--------------------------------------------------------|
                           |
                     Orange Pi 5 Plus
```

