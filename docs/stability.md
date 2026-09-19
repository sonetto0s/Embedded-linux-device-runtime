# stability文件说明

## 本文件用以记录MiniShell在ARM设备上的运行稳定性验证方式以及V1.6稳定性测试结果

## Phase 9目标

V1.6前面的Phase已经完成:

```
Orange Pi 5 Plus原生运行
ARM64支持
Device Tree读取
Hardware Info读取
sysfs访问
板载LED控制
部署以及rootfs安装
```

Phase 9不继续增加新的业务功能.

当前阶段主要用于验证MiniShell在Orange Pi 5 Plus上的运行稳定性以及资源生命周期.

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
```

硬件信息组包括:

```
sysinfo
dtinfo
hwinfo
led list
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

V1.6当前结果:

```
FD start == FD end
PASS
```

说明当前压力路径中没有发现持续文件描述符泄漏.

主要覆盖:

```
Pipe
Redirect
Event FD
Child Process
Config
sysfs
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

V1.6当前结果:

```
RSS Check PASS
```

没有发现持续异常内存增长.

## Zombie检查

大量后台任务完成以后检查MiniShell直接Child状态.

要求不存在:

```
Z
```

状态.

V1.6当前结果:

```
Zombie Check PASS
```

当前SIGCHLD、Event以及Job Reap路径可以正常完成后台Child回收.

## Config reload验证

压力测试连续执行:

```
reload
```

并且执行:

```
cd /tmp
```

以后继续reload.

V1.6当前结果:

```
Config Reload PASS
```

说明当前reload使用稳定Config路径,不会因为cwd变化失去配置来源.

## Hardware读取压力

压力循环执行:

```
sysinfo
dtinfo
hwinfo
led list
```

主要涉及:

```
/proc
/proc/device-tree
/sys/class/thermal
/sys/devices/system/cpu/cpufreq
/sys/class/net
/sys/class/leds
```

V1.6当前结果:

```
Hardware Runtime Read PASS
```

没有发现重复读取导致Shell异常退出或者资源持续增长.

## Redirect以及Pipeline压力

重复执行:

```
printf abc | wc -c
```

以及:

```
echo stable > file
cat < file > /dev/null
```

最终检查重定向文件内容.

V1.6当前结果:

```
Pipeline Stress PASS
Redirect Stress PASS
```

## Background Job压力

稳定性脚本创建大量:

```
true &
```

后台任务.

之后等待Child完成并检查Zombie状态.

V1.6当前结果:

```
Background Job Stress PASS
```

## Shell存活检查

压力测试过程中持续检查MiniShell PID.

如果Shell中途异常退出则测试立即失败.

V1.6当前结果:

```
Shell Runtime Alive PASS
```

完成全部工作负载以后Shell仍然可以正常执行最终同步命令.

## Shell退出检查

全部压力结束以后发送:

```
exit
```

等待MiniShell正常退出.

V1.6当前结果:

```
Shell Exit PASS
```

## Strict验证

执行:

```
make strict
```

启用:

```
-Wall
-Wextra
-Wpedantic
-Wformat=2
-Wstrict-prototypes
-Werror
```

V1.6当前结果:

```
Strict Build PASS
Unit Test PASS
Integration Test PASS
```

## ASan/LSan/UBSan验证

执行:

```
make clean
make asan
```

当前包含:

```
AddressSanitizer
LeakSanitizer
UndefinedBehaviorSanitizer
```

V1.6当前结果:

```
AddressSanitizer PASS
LeakSanitizer PASS
UndefinedBehaviorSanitizer PASS
```

没有发现:

```
heap-use-after-free
heap-buffer-overflow
stack-buffer-overflow
double free
invalid free
memory leak
undefined behavior
```

Orange Pi ARM64环境下Sanitizer运行速度明显低于普通Build.

PTY Integration Test原有进程退出等待时间为:

```
4000ms
```

在普通Build下可以正常完成,但是在ARM64 Sanitizer环境中Sanitizer Runtime退出检查可能超过该时间.

因此PTY Test Harness针对存在:

```
ASAN_OPTIONS
```

的Sanitizer环境将最终进程退出等待上限调整为:

```
15000ms
```

普通Integration Test仍然保持原有超时策略.

该修改只影响测试Harness等待时间,不会修改MiniShell运行逻辑以及Job Control行为.

最终结果:

```
Unit Test PASS
Integration Test PASS
ASan PASS
LSan PASS
UBSan PASS
```

## cppcheck验证

执行:

```
make cppcheck
```

用于检查:

```
warning
performance
portability
```

cppcheck作为静态分析辅助工具使用,不能代替运行时测试.

## Valgrind验证

如果目标环境已经安装Valgrind,可以执行:

```
make valgrind
```

当前Valgrind测试包含:

```
基础命令
Pipeline/Redirect
后台任务退出
大量Child Process
```

主要检查:

```
Memory Leak
Invalid Memory Access
FD Leak
```

Valgrind属于额外运行时分析工具.

核心Release Gate仍然要求ASan/LSan/UBSan以及ARM Runtime Stability通过.

## Job Control真机验证

Job Control依赖真实终端前台Process Group.

因此除了PTY Integration Test以外,V1.6在Orange Pi 5 Plus真实TTY进行最终验证.

### Ctrl+C

执行:

```
sleep 30
```

然后:

```
Ctrl+C
```

正常结果:

```
Foreground Process Group结束
MiniShell继续运行
Terminal恢复
```

当前结果:

```
PASS
```

### Ctrl+Z

执行:

```
sleep 30
```

然后:

```
Ctrl+Z
jobs
```

正常结果:

```
Job进入STOPPED
Shell重新获得Terminal
jobs可以读取Job状态
```

当前结果:

```
PASS
```

### bg

执行:

```
bg
jobs
```

正常结果:

```
Job恢复RUNNING
Shell继续保持交互
```

当前结果:

```
PASS
```

### fg

执行:

```
fg
Ctrl+C
```

正常结果:

```
Job重新获得前台Terminal
Ctrl+C结束Foreground Job
Terminal重新返回MiniShell
```

当前结果:

```
PASS
```

## Pipeline Job Control

执行:

```
sleep 30 | cat
```

然后:

```
Ctrl+Z
jobs
bg
fg
Ctrl+C
```

验证:

```
Pipeline Process Group
Foreground Terminal
SIGTSTP
SIGCONT
waitpid
Job Status
Terminal Restore
```

V1.6当前结果:

```
Pipeline Job Control PASS
```

## Phase 9最终结果

```
make check                   PASS
make strict                  PASS
make asan                    PASS

ARM Runtime Stability        PASS
FD Check                     PASS
RSS Check                    PASS
Zombie Check                 PASS
Runtime Error Check          PASS
Shell Exit                   PASS

Config Reload Stress         PASS
Pipeline Stress              PASS
Redirect Stress              PASS
Background Job Stress        PASS
Hardware Runtime Read        PASS

Ctrl+C                       PASS
Ctrl+Z                       PASS
jobs                         PASS
bg                           PASS
fg                           PASS
Pipeline Job Control         PASS
```

## Phase 9结论

V1.6已经完成Orange Pi 5 Plus真实ARM Linux环境的运行稳定性验证.

当前没有发现:

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
```

Phase 9完成以后不再继续增加V1.6功能.

后续进入:

```
Phase 10 V1.6 Release
```


