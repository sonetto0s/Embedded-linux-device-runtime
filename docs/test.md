# test文件说明

## 本文件用以记录测试指令运行结果,验证当前版本效果

## 当前版本 : V1.6

## 运行环境

- 系统:Ubuntu Linux / WSL Linux / Orange Pi Linux
- 编译方式:GCC / GNU Make / CMake
- 编程语言:C11
- 调试工具:GDB / ASan / LSan / UBSan / Valgrind / cppcheck
- ARM平台:Orange Pi 5 Plus / RK3588 / aarch64

## 基础指令测试

```
pwd:
正常输出当前工作目录


echo hello:
hello


ls:
正常输出当前目录文件


不存在指令:
返回127
Shell不会异常退出


存在但无法执行的目标:
返回126
```

## 重定向测试

```
echo hello > test.txt:
hello成功写入test文件


echo world >> test.txt:
world顺利追加至hello后


cat < test.txt:
hello
world
```

Builtin重定向:

```
pwd > test.txt
```

执行完成后Shell stdout正常恢复.

Builtin重定向失败:

```
pwd > output.txt < not_exist_file
```

即使输入重定向失败,已经修改过的stdin/stdout仍会恢复.

External重定向失败:

```
cat < not_exist_file
echo alive
```

第一条命令失败以后Shell仍然可以继续运行.

Builtin输出设备失败:

```
pwd > /dev/full
```

当前要求:

```
Builtin输出flush失败
命令返回1
stdout恢复
后续命令仍然正常
```

## Pipeline测试

```
echo hello | wc -c:
6


seq 3 | grep 2 | wc -l:
1
```

Pipeline退出状态:

```
sleep 1 | false
status
```

返回:

```
1
```

当前Pipeline退出状态使用最后一个Process状态.

不实现:

```
pipefail
```

## 后台任务测试

```
sleep 10 &
```

Shell立即返回Prompt.

```
sleep 10 &
jobs
```

可以正常输出Job状态.

多个后台任务:

```
sleep 10 &
sleep 20 &
jobs
```

JobManager可以同时管理多个Job.

后台任务完成:

```
SIGCHLD
 |
event_notify
 |
select
 |
job_reap
 |
job_cleanup_done
```

Child正常回收,不会长期留下Zombie.

## Input生命周期测试

Integration Test包含分段输入回归:

```
sleep 0.2 &
echo PART
```

这里`echo PART`暂时不发送换行.

等待后台`sleep`结束并产生SIGCHLD.

然后继续输入:

```
IAL
```

以及换行.

要求:

```
SIGCHLD不能导致PART提前执行
最终只执行PARTIAL
```

## Ctrl+C测试

前台:

```
sleep 30
```

输入:

```
Ctrl+C
```

结果:

```
Foreground Process Group结束
Shell继续运行
Terminal恢复
Prompt重新出现
```

Prompt状态Ctrl+C:

```
>>MiniShell ^C
>>MiniShell
```

当前未完成输入会被清空.

## Ctrl+Z测试

```
sleep 30
```

输入:

```
Ctrl+Z
```

结果:

```
Job状态变为STOPPED
保存Job Terminal Modes
Terminal恢复给Shell
jobs可以查看停止任务
```

## fg测试

```
sleep 30
Ctrl+Z
fg
```

结果:

```
停止Job重新获得Terminal
恢复Job Terminal Modes
SIGCONT继续运行
Shell等待Foreground Job
Job结束或者再次停止以后Terminal重新返回Shell
```

额外覆盖:

```
fg < /dev/null
```

Terminal控制使用独立:

```
/dev/tty
```

不会因为fd 0被重定向导致Foreground切换失败.

## bg测试

```
sleep 30
Ctrl+Z
bg
```

结果:

```
停止Job重新运行
Job状态变为RUNNING
Shell仍然可以继续输入
```

## Pipeline Job Control测试

```
sleep 30 | cat
```

依次:

```
Ctrl+Z
jobs
bg
fg
Ctrl+C
```

要求:

```
Pipeline共享Process Group
整个Pipeline作为一个Job
STOPPED状态正确
SIGCONT正确
Foreground Terminal正确
结束以后Shell恢复Terminal
```

## Background TTY测试

```
cat &
```

后台cat尝试读取Terminal后收到:

```
SIGTTIN
```

Job进入STOPPED状态.

Shell自身在父Shell后台启动时也遵守Foreground Process Group规则.

## FIFO Signal测试

Integration PTY测试包含:

```
cat < fifo
Ctrl+C
```

要求阻塞在FIFO open阶段的Child仍然使用正确External Signal语义并结束.

同时包含:

```
cat < fifo
Ctrl+Z
jobs
fg
Ctrl+C
```

验证:

```
Child startup signal
STOPPED状态
fg
Terminal交接
```

## Termios测试

测试程序会:

```
stty -echo -icanon
```

然后停止自身.

要求:

```
Job停止以后Shell恢复ECHO/ICANON
fg以后Job恢复停止前Terminal Modes
Job结束以后Shell再次恢复自身Terminal Modes
```

## Job Shutdown测试

Unit Test覆盖:

```
Running Process shutdown
Stopped Process shutdown
Multiple Process shutdown
Same Process Group descendant shutdown
```

同组后代测试会创建一个忽略SIGTERM的后代Process.

要求:

```
TERM整个pgid
有限等待
KILL整个pgid
Direct Child全部waitpid回收
同组后代不能残留
```

## Signal/Event Shutdown测试

关闭顺序:

```
signal_shutdown
 |
event_shut
```

关闭以后再次触发Signal不能因为self-pipe已经关闭导致异常SIGPIPE.

## FD_SETSIZE测试

测试进程预先打开大量FD.

让MiniShell创建的Event FD超过:

```
FD_SETSIZE
```

要求:

```
Shell在FD_SET前检测范围
输出明确错误
干净返回1
不能触发FD_SET越界
```

## Config测试

Config Unit Test包含:

```
默认配置
合法单行解析
非法max_job
文件加载
缺失文件
目录读取失败
事务式加载失败
```

事务式加载要求:

```
Temporary Config
 |
前面配置合法
 |
中间出现非法配置
 |
config_load失败
 |
旧MiniShellConfig完整保留
```

V1.6增加部署Config测试.

当前查找顺序:

```
MINISHELL_CONFIG
部署目录Config
开发目录Config
cwd Config
内部默认配置
```

Integration覆盖:

```
cd /tmp
reload
```

要求仍然使用Shell启动时保存的稳定Config路径.

## Ctrl+D测试

```
>>MiniShell
Ctrl+D
```

结果:

```
>>MiniShell 已退出
```

如果EOF以前存在未换行但已经完整输入的数据,Shell先处理最后一行再退出.

## sysinfo测试

```
sysinfo
```

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

Unit Test覆盖:

```
NULL参数
collect
字段基本有效性
重复collect覆盖旧结构
```

板卡专属字段属于可选信息.

## dtinfo测试

```
dtinfo
```

Orange Pi当前可以读取:

```
Model
Compatible
Boot Args
```

主要接口:

```
/proc/device-tree
```

Unit Test同时支持普通x86 Linux不存在Device Tree接口.

这种情况下:

```
available = 0
```

调用仍然成功.

## hwinfo测试

```
hwinfo
```

当前读取:

```
Thermal
CPUFreq
Network
LED
```

Unit Test检查:

```
NULL参数
collect成功
数量不超过结构上限
实际存在的硬件Entry名称有效
```

不同平台不要求存在完全相同硬件节点.

## sysfs_io测试

当前Unit Test覆盖:

```
非法参数
文本写入/读取
末尾换行处理
数字写入/读取
```

测试使用:

```
/tmp
```

普通临时文件验证公共I/O逻辑.

CI环境不需要模拟真实Orange Pi sysfs节点.

## LED Control测试

Unit Test主要覆盖输入检查:

```
NULL LED Name
空LED Name
路径分隔符
非法Trigger
空Trigger
包含空格Trigger
```

真实LED写入由Orange Pi真机测试完成.

## LED真机测试

查看全部LED:

```
led list
```

查看:

```
led info blue_led
```

管理员权限:

```
led on blue_led
led off blue_led
led trigger blue_led heartbeat
```

要求:

```
on后Brightness=max
on/off后Trigger=none
off后Brightness=0
heartbeat后Active Trigger恢复
```

写入完成后代码会重新读取sysfs状态进行验证.

## Unit Test

执行:

```bash
make test
```

Orange Pi 5 Plus当前最终结果:

```
Test Cases : 100
Assertions : 777
Passed     : 777
Failed     : 0
```

由于HardwareInfo测试会根据实际硬件接口执行部分条件Assertion,不同平台Assertion数量可能略有变化.

Test Case数量保持:

```
100
```

## Integration Test

执行:

```bash
make integration
```

当前最终结果:

```
Test Cases : 44
Assertions : 348
Passed     : 348
Failed     : 0
```

Integration Test当前包括:

```
基础命令
不存在命令
126/127状态
输入/输出/追加重定向
Builtin重定向恢复
Builtin /dev/full输出失败
Builtin输出失败恢复
Pipeline
Pipeline退出状态
后台任务
后台任务回收
Shell退出清理
稳定Config reload
高FD边界
分段输入 + SIGCHLD
PTY启动退出
Ctrl+C
Ctrl+Z
fg
bg
Pipeline Job Control
后台TTY停止
fg < /dev/null
FIFO Ctrl+C
FIFO Ctrl+Z
Job/Shell termios恢复
后台启动Terminal规则
```

## 当前测试总量

Orange Pi 5 Plus:

```
Unit Test:
100 Cases
777 Assertions

Integration Test:
44 Cases
348 Assertions

Total:
144 Cases
1125 Assertions
0 Failed
```

Assertion数量存在平台差异时以当前平台实际输出为准.

## 完整测试

执行:

```bash
make check
```

要求:

```
Unit Test PASS
Integration Test PASS
```

V1.6当前验证通过.

## Strict测试

执行:

```bash
make strict
```

编译参数:

```
-Wall
-Wextra
-Wpedantic
-Wformat=2
-Wstrict-prototypes
-Werror
```

当前V1.6验证:

```
Unit Test PASS
Integration Test PASS
0 warning
0 error
```

## ASan/LSan/UBSan测试

执行:

```bash
make asan
```

当前包含:

```
AddressSanitizer
LeakSanitizer
UndefinedBehaviorSanitizer
```

Orange Pi 5 Plus最终验证:

```
Unit Test PASS
Integration Test PASS
AddressSanitizer PASS
LeakSanitizer PASS
UndefinedBehaviorSanitizer PASS
```

当前没有发现:

```
heap-use-after-free
heap-buffer-overflow
stack-buffer-overflow
double free
invalid memory access
memory leak
undefined behavior
```

## ARM64 Sanitizer PTY超时

Orange Pi使用:

```
aarch64
GCC 9.4.0
```

Sanitizer版本运行速度明显低于普通Build.

原PTY Test Harness等待Supervisor退出时间为:

```
4000ms
```

普通Build可以正常完成.

ASan/LSan环境下MiniShell主体已经完成退出流程,但是Sanitizer Runtime退出检查可能使最终Process退出超过4秒.

因此当前wait_for_pid在检测到:

```
ASAN_OPTIONS
```

时将最终退出等待上限调整为:

```
15000ms
```

普通Integration Test仍然保持原有超时.

该修改只属于Test Harness兼容性调整.

不会修改:

```
MiniShell运行逻辑
Terminal逻辑
Job Control逻辑
Signal逻辑
```

调整以后Orange Pi:

```
make asan
```

完整通过.

## Valgrind测试

执行:

```bash
make valgrind
```

当前Makefile包含:

```
基础命令
重定向/Pipeline
后台任务退出
100 Child压力测试
```

主要检查:

```
Memory Leak
Invalid Memory Access
FD Leak
```

Valgrind属于额外运行时分析工具.

## cppcheck测试

执行:

```bash
make cppcheck
```

检查:

```
warning
performance
portability
```

## Static测试

执行:

```bash
make static
```

包括:

```
make strict
make cppcheck
```

## CMake测试

Debug:

```bash
cmake -S . -B build/cmake-debug \
    -DCMAKE_BUILD_TYPE=Debug \
    -DMINISHELL_WARNINGS_AS_ERRORS=ON

cmake --build build/cmake-debug --parallel

ctest --test-dir build/cmake-debug --output-on-failure
```

Release:

```bash
cmake -S . -B build/cmake-release \
    -DCMAKE_BUILD_TYPE=Release \
    -DMINISHELL_WARNINGS_AS_ERRORS=ON

cmake --build build/cmake-release --parallel

ctest --test-dir build/cmake-release --output-on-failure
```

当前Make/CMake均包含V1.6新增模块:

```
device_tree
hardware_info
sysfs_io
led_control
```

CTest同时执行:

```
Unit Test
Integration Test
```

## ARM64测试

交叉编译:

```bash
make arm64
```

检查:

```bash
file build/arm64/minishell
```

部署包:

```bash
make arm64-package
```

V1.6除ARM64交叉编译以外,已经完成Orange Pi真实aarch64平台原生运行.

## Deployment测试

Make临时rootfs:

```bash
make install DESTDIR=/tmp/minishell-root
```

要求:

```
/tmp/minishell-root/opt/minishell/bin/minishell
/tmp/minishell-root/opt/minishell/config/config.conf
```

权限:

```
minishell    0755
config.conf  0644
```

离开源码目录:

```bash
cd /tmp

/tmp/minishell-root/opt/minishell/bin/minishell
```

程序仍然可以正常启动以及读取部署Config.

CMake同样验证:

```bash
DESTDIR=/tmp/minishell-cmake-root \
cmake --install build/cmake --prefix /opt/minishell
```

Make以及CMake使用相同目标目录结构.

## ARM Runtime Stability测试

脚本:

```bash
./tests/stability/arm_runtime_stability.sh
```

当前压力:

```
Foreground Command
Pipeline
Redirect
Config reload
Background Job
sysinfo
dtinfo
hwinfo
led list
```

检查:

```
Shell存活
FD回到基线
RSS增长范围
Zombie Child
Redirect结果
Runtime错误
Shell正常退出
```

Orange Pi 5 Plus当前结果:

```
ARM Runtime Stability PASS
FD Check PASS
RSS Check PASS
Zombie Check PASS
Runtime Error Check PASS
Shell Exit PASS
```

详细说明:

```
docs/stability.md
```

## Orange Pi Job Control测试

真实TTY环境验证:

```
Ctrl+C
Ctrl+Z
jobs
bg
fg
Pipeline Job Control
```

典型流程:

```
sleep 30
Ctrl+C
```

```
sleep 30
Ctrl+Z
jobs
bg
fg
Ctrl+C
```

```
sleep 30 | cat
Ctrl+Z
jobs
bg
fg
Ctrl+C
```

当前全部通过.

## Final Gate

V1.6正式Release前最终执行:

```
make clean
make check

make strict

make clean
make asan

make cppcheck

make clean
make

./tests/stability/arm_runtime_stability.sh

CMake Debug
CMake Release
make package
make install DESTDIR
git diff --check
GitHub Actions
```

如果当前环境安装Valgrind:

```
make valgrind
```

Orange Pi真机额外确认:

```
sysinfo
dtinfo
hwinfo
led list
LED Control
Ctrl+C
Ctrl+Z
jobs
bg
fg
Pipeline Job Control
```

## 当前测试结论

```
普通功能测试通过
Unit Test通过
Integration Test通过
Input生命周期测试通过
PTY Job Control测试通过
Orange Pi真实TTY Job Control通过
Signal/Event测试通过
Terminal/termios测试通过
Config事务测试通过
Config部署路径测试通过
高FD边界测试通过
System Info测试通过
Device Tree测试通过
Hardware Info测试通过
sysfs_io测试通过
LED Control测试通过
Deployment测试通过
ARM Runtime Stability测试通过
FD稳定性测试通过
Zombie检查通过
ASan/LSan/UBSan通过
-Werror严格编译通过
```

V1.6当前测试以及ARM平台稳定性验证完成
