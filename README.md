# myminishell

## 项目简介
👋👋
这是一个基于Linux用户态实现的MiniShell,主要用来实现命令解析、进程控制、管道、重定向、Job Control、Signal/Event等功能,以此深入学习Linux系统编程相关机制🙃
项目目前已逐步增加模块化、配置、日志、错误处理、Job管理、自动化测试、ARM Linux运行以及硬件运行时信息读取等工程化能力.
V1.6已经正式迁移至ARM Linux/Orange Pi 5 Plus环境,并开始向嵌入式Linux设备管理终端方向扩展.

## 开发环境

- OS: Ubuntu Linux / Orange Pi Linux
- compiler: GCC
- 编译工具: GNU Make / CMake
- 编程语言: C11
- ARM平台: Orange Pi 5 Plus / RK3588 / aarch64

## 当前版本:V1.6 ARM / Orange Pi

## 更新日志

V1.6:
- MiniShell正式迁移至Orange Pi 5 Plus ARM Linux环境
- 扩展system_info模块,新增Board,CPU Core以及SoC Temperature信息
- 新增device_tree模块,读取Device Tree Runtime信息
- 新增hardware_info模块,读取Thermal,CPUFreq,Network以及LED状态,新增hwinfo内建命令
- 新增Make install/uninstall
- 支持PREFIX以及DESTDIR
- 完成Orange Pi真实TTY环境Job Control验证

V1.5:
- 优化signal/event/terminal/job control
- 完善Foreground/Background Process Group管理
- 引入自动化测试/Sanitizer/Valgrind
- 新增CI/ARM Linux交叉编译链
- 引入ASan/LSan/UBSan/Valgrind/cppcheck等检查流程
- 新增CMake构建并验证

V1.4:
- 增加process groups
- 优化terminal与job control
- 增加fg与bg等系统操作功能

V1.3:
- 优化工程稳定性
- 优化job/event/executor/shell/errno层代码

V1.2:
- 新增system_info模块,识别设备状态
- 优化原有config/logger文件实现

V1.1:
- 新增builtin_table文件,优化原有代码逻辑
- table框架新增识别help/job/status功能
- 优化原有状态管理

V1.0:
- 完成MiniShell工程化重构
- 新增Config配置管理模块
- 新增Error错误处理模块
- 新增Log日志系统
- 新增基础框架测试

V0.9.3:
- 完善select构建基础事件循环
- 完善ShellContext统一管理Shell状态

V0.9.2:
- 引入事件驱动模型
- 实现select监听多路
- 重构sigchld处理流程

V0.9.1:
- 重构Shell生命周期控制机制,实现Shell运行状态统一管理
- 优化exit退出流程,实现状态驱动退出

V0.9:
- 初步开始MiniShell工程化重构
- 优化模块职责划分,提高系统可扩展性

V0.8.2:
- 新增job_list/job_remove函数,基本实现后台任务管理
- 优化原有sigchld函数实现

V0.8.1:
- 引入job,记录运行数据信息
- 初步完成后台任务管理

V0.8:
- 新增后台管理机制
- 支持&符号后台执行

V0.7.4:
- 新增test.md文件,用以初步检测现有指令解析处理功能

V0.7.3:
- 暂时注释sigchld,减少进程回收冲突
- 新增Ctrl+D指令实现,优化Ctrl+C指令实现
- 新增内存释放函数,修补缺失功能

V0.7.2:
- 完善$?机制,完善Shell功能
- 修复Pipeline返回值

V0.7.1:
- 修补代码逻辑漏洞,修复原有追加重定向缺失

V0.7:
- 优化execute进程处理逻辑,实现返回退出码
- 新增shell_context文件,处理Shell status值

V0.6.2:
- 优化Signal机制,引入SIGCHLD
- 调整原有execute函数进程逻辑

V0.6.1:
- 更新sig函数,优化信号处理方式
- 实现父子进程Signal区别接受处理

V0.6:
- 引入Signal机制
- 实现Ctrl+C指令功能

V0.5.3:
- 优化原有命令管道解析,升级为可识别多重管道
- 优化executor文件逻辑实现

V0.5.2:
- 剥离redirect,优化代码逻辑

V0.5.1:
- 优化原有executor解析函数
- 完整实现pipe管道与指令分析功能

V0.5:
- 新增识别"|",优化符号识别
- 引入管道pipe,解析多重指令
- 修改原有parse函数类型为结构体

V0.4.2:
- 继续优化tokenize函数,引入多种符号检测
- 持续精简模块,剔除无用逻辑

V0.4.1:
- 新增识别">>"符号,持续优化识别逻辑

V0.4:
- 新增command文件,新增command结构体取代原有指令解析结构体
- 新增重定向部分,可识别"<>"等符号,优化命令解析逻辑

V0.3.1:
- 编写cd/pwd/exit等初等内部指令,完善builtin文件
- 完善dispatcher逻辑,补全细节
- 测试运行

V0.3:
- 新增dispatcher骨架,优化原有判断逻辑
- 新增builtin内建命令解析函数,区分原有execute函数

V0.2.1:
- 新增tokenize,取缔原有strtok函数,优化指令解析能力
- 新增Token/TokenList结构体,引入工程化模板
- 引入execvp函数,正式开始执行进程Shell

V0.2:
- 新增parser文件,用以解析指令输入
- 实现初步切割指令,识别exit等基础指令

V0.1:
- 初始化项目
- 初步构建MiniShell生命周期框架

## 命令执行

当前版本支持:

```
普通外部命令
单命令执行
多级Pipeline
输入重定向 <
输出重定向 >
追加重定向 >>
后台执行 &
Job Control
内建命令
```

示例:

```
ls -l
echo hello > output.txt
echo world >> output.txt
cat < output.txt
seq 3 | grep 2 | wc -l
sleep 10 &
```

当前内建命令:

```
cd
pwd
exit
jobs
help
status
sysinfo
dtinfo
hwinfo
led
fg
bg
reload
```

## 项目主要核心机制

### Job Control

```
MiniShell已实现基础Unix Job Control

主要能力:
- Process Group Control
- pgid管理
- Foreground/Background Job
- Ctrl+C
- Ctrl+Z
- fg
- bg
- jobs
- 前台终端所有权切换
- STOPPED/RUNNING/DONE状态管理
- Pipeline整体作为一个Job管理
- Shell退出时后台Job清理

Job状态:
- JOB_RUNNING
- JOB_STOPPED
- JOB_DONE

单个Process状态:
- PROCESS_RUNNING
- PROCESS_STOPPED
- PROCESS_DONE
```

### Signal/Event

```
MiniShell不在Signal Handler中执行复杂逻辑

当前工作流程:

Signal
   |
sig_atomic_t pending event
   |
self-pipe
   |
select()
   |
normal process context
   |
job_reap / prompt handling

处理的主要事件包括:

- SIGCHLD
- SIGINT

Shell自身忽略Job Control相关终端Signal:

- SIGQUIT
- SIGTSTP
- SIGTTIN
- SIGTTOU

Child在execvp前恢复默认Signal disposition以及Signal mask
```

### Event Loop

```
Shell主循环使用:

select()

同时监听:

STDIN
Event self-pipe
```

## 配置系统

默认开发配置文件:

```
config/config.conf
```

当前默认配置:

```
prompts=MiniShell
max_job=64
debug=0
```

V1.6配置查找顺序:

```
MINISHELL_CONFIG
 |
程序部署目录下的config/config.conf
 |
开发环境config/config.conf
 |
当前工作目录config/config.conf
 |
程序内部默认配置
```

MiniShell启动以后会保存稳定Config路径.

因此执行:

```
cd /tmp
reload
```

仍然能够读取启动时确定的Config.

## System Info

```
sysinfo
```

用于读取当前系统以及板卡基础状态:

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

Orange Pi平台可以读取:

```
RK3588 OPi 5 Plus
```

## Device Tree

V1.6新增:

```
dtinfo
```

主要读取:

```
/proc/device-tree/model
/proc/device-tree/compatible
/proc/device-tree/chosen/bootargs
```

在不存在Device Tree接口的平台上不会把该情况视为Shell致命错误.

## Hardware Info

V1.6新增:

```
hwinfo
```

当前读取:

```
Thermal Zone
CPUFreq Policy
Network Interface
LED
```

主要接口:

```
/sys/class/thermal
/sys/devices/system/cpu/cpufreq
/sys/class/net
/sys/class/leds
```

## Sysfs访问

V1.6新增公共sysfs访问层:

```
include/sysfs_io.h
src/sysfs_io.c
```

当前提供:

```
sysfs_read_text
sysfs_read_long
sysfs_read_ulong
sysfs_write_text
```

hardware_info以及led_control共同使用该模块完成底层sysfs访问.

## LED Control

V1.6新增:

```
led
```

当前支持:

```
led list
led info <name>
led on <name>
led off <name>
led trigger <name> <trigger>
```

Orange Pi板载LED可以通过:

```
blue_led
green_led
```

进行读取以及控制.

写入Trigger或者Brightness以后会重新读取sysfs进行结果确认.

涉及写操作时仍然遵守Linux自身权限机制.

## Orange Pi 5 Plus

V1.6已经在真实Orange Pi 5 Plus环境完成运行验证.

当前环境:

```
Board: RK3588 OPi 5 Plus
SoC: Rockchip RK3588
Architecture: aarch64
CPU Core: 8
Kernel: 6.1.43-rockchip-rk3588
OS: Orange Pi 1.2.0 Focal
GCC: 9.4.0
```

当前已经完成:

```
ARM64原生编译
MiniShell板端运行
System Info
Device Tree
Thermal
CPUFreq
Network
LED读取
LED控制
Rootfs部署
ARM Runtime Stability
真实TTY Job Control
ARM64 Sanitizer
```

详细说明:

```
docs/orangepi5plus.md
```

## 部署

默认安装结构:

```
/opt/minishell/
├── bin/
│   └── minishell
└── config/
    └── config.conf
```

Make安装:

```
make
sudo make install
```

临时rootfs:

```
make install DESTDIR=/tmp/minishell-root
```

卸载:

```
sudo make uninstall
```

CMake安装:

```
cmake --install build/cmake --prefix /opt/minishell
```

详细说明:

```
docs/deployment.md
```

## 运行说明

开发环境:

```
Linux
GCC
C11
Visual Studio Code
GNU Make
CMake
Orange Pi 5 Plus
```

默认构建:

```
make
```

默认生成:

```
build/default/minishell
./shell
```

运行:

```
./shell
```

或者:

```
make run
```

## Build方式

普通Debug Build:

```
make debug
```

Release Build:

```
make release
```

严格编译:

```
make strict
```

严格编译会启用:

```
-Wall
-Wextra
-Wpedantic
-Wformat=2
-Wstrict-prototypes
-Werror
```

ARM64交叉编译:

```
make arm64
```

Native Package:

```
make package
```

ARM64 Package:

```
make arm64-package
```

安装:

```
make install
```

## CMake

Debug构建:

```bash
cmake -S . -B build/cmake-debug \
    -DCMAKE_BUILD_TYPE=Debug \
    -DMINISHELL_WARNINGS_AS_ERRORS=ON

cmake --build build/cmake-debug --parallel

ctest --test-dir build/cmake-debug --output-on-failure
```

Release构建:

```bash
cmake -S . -B build/cmake-release \
    -DCMAKE_BUILD_TYPE=Release \
    -DMINISHELL_WARNINGS_AS_ERRORS=ON

cmake --build build/cmake-release --parallel

ctest --test-dir build/cmake-release --output-on-failure
```

## Test说明

完整普通测试:

```
make check
```

Unit Test:

```
make test
```

Integration Test:

```
make integration
```

Sanitizer:

```
make asan
```

Strict:

```
make strict
```

静态分析:

```
make cppcheck
```

ARM Runtime Stability:

```
./tests/stability/arm_runtime_stability.sh
```

详细测试说明见以下文件:

```
docs/test.md
docs/stability.md
```

## 当前测试状态

Orange Pi 5 Plus当前最终验证:

```
Unit Test:

100 Cases
777 Assertions
0 Failed


Integration Test:

44 Cases
348 Assertions
0 Failed
```

同时完成:

```
make check
make strict
make asan
ARM Runtime Stability
Orange Pi真实TTY Job Control
```

不同运行平台存在可选硬件接口差异,Assertion数量可能根据实际硬件状态略有不同,Test Case数量保持一致.

## 项目结构

```
.
├── CMakeLists.txt
├── Makefile
├── README.md
├── common
│   ├── error.c
│   ├── error.h
│   ├── log.c
│   ├── log.h
│   ├── utils.c
│   └── utils.h
├── config
│   ├── config.c
│   ├── config.conf
│   └── config.h
├── docs
│   ├── architecture.md
│   ├── debug_log.md
│   ├── deployment.md
│   ├── module.md
│   ├── orangepi5plus.md
│   ├── stability.md
│   └── test.md
├── include
│   ├── builtin.h
│   ├── builtin_table.h
│   ├── command.h
│   ├── device_tree.h
│   ├── dispatcher.h
│   ├── event.h
│   ├── executor.h
│   ├── hardware_info.h
│   ├── job.h
│   ├── led_control.h
│   ├── parser.h
│   ├── shell.h
│   ├── shell_context.h
│   ├── sig.h
│   ├── sysfs_io.h
│   ├── system_info.h
│   └── terminal.h
├── src
│   ├── builtin.c
│   ├── builtin_table.c
│   ├── command.c
│   ├── device_tree.c
│   ├── dispatcher.c
│   ├── event.c
│   ├── executor.c
│   ├── hardware_info.c
│   ├── job.c
│   ├── led_control.c
│   ├── main.c
│   ├── parser.c
│   ├── shell.c
│   ├── shell_context.c
│   ├── sig.c
│   ├── sysfs_io.c
│   ├── system_info.c
│   └── terminal.c
└── tests
    ├── integration
    │   ├── test_integration_main.c
    │   ├── test_shell_background.c
    │   ├── test_shell_basic.c
    │   ├── test_shell_input.c
    │   ├── test_shell_pipeline.c
    │   ├── test_shell_pty.c
    │   ├── test_shell_redirect.c
    │   ├── test_shell_runner.c
    │   └── test_shell_status.c
    ├── stability
    │   └── arm_runtime_stability.sh
    ├── test_builtin_table.c
    ├── test_command.c
    ├── test_config.c
    ├── test_config.conf
    ├── test_device_tree.c
    ├── test_dispatcher.c
    ├── test_event.c
    ├── test_executor.c
    ├── test_framework.c
    ├── test_framework.h
    ├── test_hardware_info.c
    ├── test_job.c
    ├── test_job_control.c
    ├── test_led_control.c
    ├── test_log.c
    ├── test_main.c
    ├── test_parser.c
    ├── test_shell_context.c
    ├── test_sysfs_io.c
    └── test_system_info.c
```

## 技术栈
- C语言(C11)
- Linux应用/系统编程
- fork/exec/wait/waitpid
- Process Group / Job Control
- Pipe IPC
- Signal
- self-pipe
- select
- TTY/termios
- Device Tree
- sysfs
- Linux LED subsystem
- ARM64/aarch64
- GNU Make
- CMake
- GDB
- ASan/LSan/UBSan
- Valgrind
- cppcheck
- Git/GitHub Actions
- ARM64 Cross Compile

## 后续方向

V1.6完成以后,MiniShell已经从纯PC/Linux用户态工程进入真实ARM Linux环境.

后续方向:

```
Device Layer
UART
设备通信
设备状态管理
Event Driven Device IO
Linux Driver基础
嵌入式设备管理终端
```

## 当前项目roadmap

```
V1.2 Basic Stable                    OK
V1.3 Stability                       OK
V1.4 Unix Depth                      OK

V1.5 Engineering Release             OK
 ├── Correctness / Build Baseline    OK
 ├── Test Framework                  OK
 ├── Unit Test Expansion             OK
 ├── Integration / System Test       OK
 ├── Resource Lifetime               OK
 ├── Job / Signal / Terminal         OK
 ├── Runtime Analysis                OK
 ├── Static Analysis                 OK
 ├── Documentation                   OK
 ├── CI                              OK
 ├── ARM-ready Build Interface       OK
 ├── CMake Build Parity              OK
 └── Release Candidate Audit         OK

V1.6 ARM / Orange Pi                 OK
 ├── Orange Pi Bring-up              OK
 ├── Embedded System Information     OK
 ├── Deployment Runtime              OK
 ├── Board Interface Audit           OK
 ├── Device Tree Runtime Inspector   OK
 ├── Hardware Runtime Inspector      OK
 ├── Sysfs Control Layer / LED       OK
 ├── Deployment / Install            OK
 ├── ARM Runtime Stability           OK
 └── Release Audit                   OK

V1.7 Device Layer
V1.8 Event Driven Device IO
V1.9 Embedded Terminal Integration
V2.0 Embedded Device Terminal
```

