# stability文件说明

## 本文件用以记录MiniShell在ARM设备上的运行稳定性验证方式以及当前稳定性测试内容

## 当前目标

当前稳定性测试不继续增加新的Shell功能.

主要用于验证MiniShell在Orange Pi 5 Plus中持续运行时的资源生命周期以及Runtime读取稳定性.

主要检查:

```
进程生命周期
文件描述符
内存增长
SIGCHLD
后台任务回收
Job Control
Pipeline
Redirect
Config reload
Device Tree
Hardware Info
Runtime Monitor
Process Monitor
sysfs读取
Shell退出清理
```

## 自动稳定性验证

自动稳定性脚本位于:

```
tests/stability/arm_runtime_stability.sh
```

执行:

```
make

./tests/stability/arm_runtime_stability.sh
```

脚本启动一个持续运行的MiniShell进程,并在同一个进程内连续执行大量操作.

当前压力内容:

```
前台命令        300次
Pipeline        100次
重定向          200次
配置reload      150次
后台进程        200次
硬件信息读取    20组
Runtime读取     20组
```

硬件信息组包括:

```
sysinfo
dtinfo
hwinfo
led list
```

Runtime组包括:

```
monitor
psinfo <MiniShell PID>
```

脚本使用FIFO向同一个MiniShell持续发送Command.

测试结束或者中途失败时通过cleanup清理:

```
MiniShell进程
FIFO FD
/tmp临时目录
```

## FD检查

测试开始以后读取:

```
/proc/<pid>/fd
```

记录:

```
FD start
```

完成全部压力操作以后再次读取:

```
FD end
```

要求:

```
FD start == FD end
```

主要覆盖:

```
Pipe
Redirect
Event FD
Child Process
Config
/proc
sysfs
Runtime Monitor
Process Monitor
```

如果FD无法回到基线:

```
FAIL
```

## RSS检查

测试通过:

```
/proc/<pid>/status
```

读取:

```
VmRSS
```

运行前后RSS允许存在一定变化.

原因包括:

```
glibc allocator
stdio buffer
动态链接器
运行时缓存
```

因此不要求:

```
RSS start == RSS end
```

当前稳定性脚本限制明显异常增长.

如果增长超过脚本设置的范围:

```
FAIL
```

RSS检查主要用于发现持续异常增长.

具体Memory错误继续由:

```
ASan
LSan
Valgrind
```

检查.

## Zombie检查

大量后台任务完成以后检查MiniShell直接Child状态.

使用:

```
ps -o stat= --ppid <MiniShell PID>
```

要求不存在:

```
Z
```

状态.

如果发现Zombie:

```
FAIL
```

## Config reload验证

重复执行:

```
reload
```

然后:

```
cd /tmp
```

继续执行reload.

主要检查:

```
稳定Config Path
cwd变化
重复Config Load
```

不会因为当前工作目录改变导致reload找不到启动时确定的Config.

## Hardware读取压力

重复执行:

```
sysinfo
dtinfo
hwinfo
led list
```

主要覆盖:

```
/proc
/proc/device-tree
/sys/class/thermal
/sys/devices/system/cpu/cpufreq
/sys/class/net
/sys/class/leds
```

稳定性脚本只读取LED状态.

不会自动执行LED写操作.

## Runtime读取压力

重复执行:

```
monitor
psinfo <MiniShell PID>
```

monitor主要覆盖:

```
runtime_monitor
runtime_snapshot
thermal_monitor
network_monitor
```

接口:

```
/proc/stat
/proc/meminfo
/proc/loadavg
/proc/uptime
/proc
/sys/class/thermal
/sys/class/net
```

psinfo主要覆盖:

```
process_monitor
/proc/<pid>/status
```

重复读取过程中不应出现:

```
FD持续增长
明显RSS持续增长
Runtime读取异常
Shell异常退出
```

## Redirect以及Pipeline压力

Pipeline重复:

```
printf abc | wc -c
```

Redirect重复:

```
echo stable > file
cat < file > /dev/null
```

最终检查Redirect File内容:

```
stable
```

主要覆盖:

```
pipe
fork
Process Group
open
dup2
FD关闭
waitpid
```

## Background Job压力

重复执行:

```
true &
```

主要覆盖:

```
SIGCHLD
self-pipe
job_reap
job_cleanup_done
```

全部后台Process结束以后继续进行Zombie检查.

## Shell存活检查

全部压力Command执行完成以后发送Marker:

```
__MINISHELL_ARM_STABILITY_DONE__
```

脚本等待Marker出现在Log中.

之后检查:

```
kill -0 <MiniShell PID>
```

MiniShell必须仍然存在.

如果中途退出:

```
FAIL
```

## Shell退出检查

全部检查完成以后发送:

```
exit
```

等待MiniShell退出.

最终要求:

```
Exit Status = 0
```

## Strict验证

执行:

```
make strict
```

要求:

```
Unit Test PASS
Integration Test PASS
0 warning
0 error
```

## ASan/LSan/UBSan验证

执行:

```
make asan
```

当前包括:

```
AddressSanitizer
LeakSanitizer
UndefinedBehaviorSanitizer
```

主要检查:

```
heap-use-after-free
buffer overflow
double free
invalid free
memory leak
undefined behavior
```

Orange Pi ARM64中Sanitizer运行速度比普通Build慢.

PTY Test Harness在Sanitizer环境中保留更长的最终Process退出等待时间.

该调整只影响Test Harness.

## cppcheck验证

执行:

```
make cppcheck
```

主要检查:

```
warning
performance
portability
```

## Valgrind验证

执行:

```
make valgrind
```

主要检查:

```
Memory Leak
Invalid Memory Access
FD Leak
```

Valgrind运行速度较慢,主要在Release阶段作为附加检查.

## Job Control真机验证

Stability Script使用FIFO.

因此不能完全代替真实TTY Job Control验证.

### Ctrl+C

```
sleep 30
Ctrl+C
```

要求:

```
Foreground Process Group结束
Shell继续运行
Terminal恢复
```

### Ctrl+Z

```
sleep 30
Ctrl+Z
```

要求:

```
Job STOPPED
Terminal返回Shell
jobs可以查看Job
```

### bg

```
sleep 30
Ctrl+Z
bg
```

要求:

```
SIGCONT
Job RUNNING
Shell继续运行
```

### fg

```
sleep 30
Ctrl+Z
fg
```

要求:

```
Job重新获得Terminal
恢复Job Terminal Modes
Job继续运行
```

## Pipeline Job Control

```
sleep 30 | cat
Ctrl+Z
jobs
bg
fg
Ctrl+C
```

主要检查:

```
Pipeline Process Group
Foreground Terminal
SIGTSTP
SIGCONT
waitpid
Job Status
Terminal Restore
```

## 当前最终检查

```
make check
make strict
make asan
CMake Debug
CMake Release

ARM Runtime Stability
FD Check
RSS Check
Zombie Check
Runtime Error Check
Shell Exit

Config Reload Stress
Pipeline Stress
Redirect Stress
Background Job Stress
Hardware Runtime Read
Runtime Monitor Read
Process Monitor Read

Ctrl+C
Ctrl+Z
jobs
bg
fg
Pipeline Job Control
```

## 当前结论

稳定性检查主要用于确认当前MiniShell在真实ARM Linux环境长期运行时没有明显资源生命周期问题.

最终重点检查:

```
持续FD泄漏
明显RSS异常增长
Zombie Child残留
Shell压力过程异常退出
Sanitizer内存错误
Job Control失效
Terminal无法恢复
Config reload路径失效
Hardware Info重复读取异常
Runtime重复读取异常
```

Release阶段同时结合:

```
Unit Test
Integration Test
Sanitizer
Valgrind
CMake
GitHub Actions
真实TTY测试
```

