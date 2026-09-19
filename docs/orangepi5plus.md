# orangepi5plus文件说明

## 本文件用以记录MiniShell在Orange Pi 5 Plus ARM Linux平台下的开发以及运行情况

## 当前开发环境

```
Board: RK3588 OPi 5 Plus
SoC: Rockchip RK3588
Architecture: aarch64
CPU Core: 8
Kernel: 6.1.43-rockchip-rk3588
OS: Orange Pi 1.2.0 Focal
GCC: 9.4.0
GNU Make: 4.2.1
```

Device Tree:

```
/proc/device-tree/model

RK3588 OPi 5 Plus
```

Compatible:

```
rockchip,rk3588-orangepi-5-plus
rockchip,rk3588
```

## ARM64原生编译

V1.6正式在Orange Pi 5 Plus中进行ARM64原生编译以及运行.

编译:

```
make clean
make
```

Release Build:

```
make release
```

运行:

```
./shell
```

当前MiniShell已经可以正常执行:

```
外部命令
Pipeline
Redirect
Background Job
Job Control
Builtin
System Info
Device Tree
Hardware Info
LED Control
```

## System Info

执行:

```
sysinfo
```

当前Orange Pi可以读取:

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

Board读取:

```
/proc/device-tree/model
```

当前:

```
RK3588 OPi 5 Plus
```

CPU Core通过:

```
sysconf(_SC_NPROCESSORS_ONLN)
```

读取.

当前:

```
8
```

SoC Temperature通过:

```
/sys/class/thermal
```

中的:

```
soc-thermal
```

获取.

## Device Tree

V1.6新增Device Tree Runtime Inspector.

执行:

```
dtinfo
```

主要读取:

```
/proc/device-tree/model
/proc/device-tree/compatible
/proc/device-tree/chosen/bootargs
```

当前Model:

```
RK3588 OPi 5 Plus
```

当前Compatible:

```
rockchip,rk3588-orangepi-5-plus
rockchip,rk3588
```

Device Tree信息通过运行时文件读取.

MiniShell不需要重新解析dtb文件.

## Thermal接口

RK3588当前提供以下Thermal Zone:

```
soc-thermal
bigcore0-thermal
bigcore1-thermal
littlecore-thermal
center-thermal
gpu-thermal
npu-thermal
```

相关接口:

```
/sys/class/thermal/thermal_zone*
```

hwinfo会遍历当前实际存在的Thermal Zone.

## CPUFreq接口

当前主要Policy:

```
policy0
policy4
policy6
```

接口:

```
/sys/devices/system/cpu/cpufreq
```

当前可以读取:

```
affected_cpus
scaling_driver
scaling_governor
scaling_cur_freq
scaling_min_freq
scaling_max_freq
cpuinfo_min_freq
cpuinfo_max_freq
```

当前Driver:

```
cpufreq-dt
```

当前Governor:

```
ondemand
```

不同CPU Cluster具有不同频率范围.

## Network接口

相关接口:

```
/sys/class/net
```

当前主要接口包括:

```
enP3p49s0
enP4p65s0
lo
```

hwinfo读取:

```
Interface Name
Operstate
MAC Address
MTU
```

网络状态以运行时sysfs结果为准.

## LED接口

Orange Pi当前存在:

```
blue_led
green_led
mmc0::
```

接口:

```
/sys/class/leds
```

当前读取:

```
brightness
max_brightness
trigger
```

板载GPIO LED:

```
blue_led
green_led
```

当前max_brightness:

```
1
```

V1.6使用:

```
led list
```

查看全部LED.

查看单个LED:

```
led info blue_led
```

## Sysfs访问

V1.6增加公共访问层:

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

Hardware Info以及LED Control不再各自实现重复底层文件访问.

sysfs文件打开使用:

```
O_CLOEXEC
```

并处理:

```
EINTR
```

## Hardware Info

执行:

```
hwinfo
```

当前统一输出:

```
Thermal Zones
CPU Frequency
Network
LEDs
```

内部流程:

```
hwinfo
 |
hardware_info_collect
 |
sysfs_io
 |
Linux sysfs
```

## LED Control

查看:

```
led list
led info blue_led
```

管理员权限控制:

```
led on blue_led
led off blue_led
led trigger blue_led heartbeat
```

on/off执行以前会切换:

```
trigger=none
```

然后修改:

```
brightness
```

写入以后会重新读取:

```
trigger
brightness
```

确认系统实际状态.

普通用户没有sysfs写权限时返回:

```
Permission denied
```

MiniShell不会自动sudo或者修改系统权限.

## Board Interface Audit

V1.6已经对Orange Pi当前Linux接口进行检查.

GPIO:

```
/dev/gpiochip0
/dev/gpiochip1
/dev/gpiochip2
/dev/gpiochip3
/dev/gpiochip4
/dev/gpiochip5
```

I2C:

```
/dev/i2c-0
/dev/i2c-1
/dev/i2c-3
/dev/i2c-6
/dev/i2c-7
/dev/i2c-9
/dev/i2c-10
/dev/i2c-11
```

SPI:

```
当前无/dev/spidev*
```

Serial:

```
/dev/ttyFIQ0
/dev/ttyS9
```

这些接口在V1.6只进行Audit.

V1.6不继续加入:

```
UART Device Layer
I2C Device Layer
SPI Device Layer
外接GPIO设备
```

相关设备功能留到后续版本.

## Config部署路径

V1.6配置查找顺序:

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

默认正式安装结构:

```
/opt/minishell/
├── bin/
│   └── minishell
└── config/
    └── config.conf
```

因此程序位于:

```
/opt/minishell/bin/minishell
```

时可以自动找到:

```
/opt/minishell/config/config.conf
```

即使运行过程中:

```
cd /tmp
```

后续:

```
reload
```

仍然使用启动时保存的稳定Config路径.

## Rootfs部署

Make暂存安装:

```
make install DESTDIR=/tmp/minishell-root
```

生成:

```
/tmp/minishell-root/opt/minishell/bin/minishell
/tmp/minishell-root/opt/minishell/config/config.conf
```

程序权限:

```
0755
```

Config权限:

```
0644
```

CMake同样支持:

```
DESTDIR=/tmp/minishell-root \
cmake --install build/cmake --prefix /opt/minishell
```

详细说明:

```
docs/deployment.md
```

## ARM Runtime Stability

V1.6新增:

```
tests/stability/arm_runtime_stability.sh
```

自动压力内容:

```
Foreground Command        300次
Pipeline                  100次
Redirect                  200次
Config reload             150次
Background Job            200次
Hardware Info             20组
```

运行过程中检查:

```
FD
RSS
Zombie
Shell存活
Redirect结果
Shell正常退出
```

Orange Pi最终结果:

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

## Job Control真机验证

Orange Pi真实TTY环境完成:

```
Ctrl+C
Ctrl+Z
jobs
bg
fg
Pipeline Job Control
```

Ctrl+C:

```
sleep 30
Ctrl+C
```

结果:

```
Foreground Job结束
MiniShell继续运行
Terminal恢复
```

Ctrl+Z/fg/bg:

```
sleep 30
Ctrl+Z
jobs
bg
fg
Ctrl+C
```

结果:

```
STOPPED状态正常
SIGCONT正常
Foreground切换正常
Terminal恢复正常
```

Pipeline:

```
sleep 30 | cat
Ctrl+Z
jobs
bg
fg
Ctrl+C
```

整个Pipeline共享Process Group并作为一个Job进行控制.

## ARM64 Sanitizer

Orange Pi环境:

```
aarch64
GCC 9.4.0
```

执行:

```
make clean
make asan
```

最终:

```
Unit Test PASS
Integration Test PASS
AddressSanitizer PASS
LeakSanitizer PASS
UndefinedBehaviorSanitizer PASS
```

ARM64 Sanitizer退出速度明显慢于普通Build.

PTY Test Harness在检测到:

```
ASAN_OPTIONS
```

时会为最终Process退出提供更长等待时间.

该调整只影响测试Harness.

不会影响MiniShell正常运行逻辑.

## 当前测试结果

Orange Pi 5 Plus:

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

不同平台由于可选Hardware Info接口不同,Assertion数量可能存在少量变化.

## V1.6最终状态

```
Orange Pi 5 Plus平台确认         PASS
ARM64原生编译                   PASS
MiniShell板端运行               PASS
System Info                     PASS
Device Tree                     PASS
Thermal                         PASS
CPUFreq                         PASS
Network                         PASS
LED读取                         PASS
LED控制                         PASS
Sysfs访问层                     PASS
Config部署路径                  PASS
Make部署                        PASS
CMake部署                       PASS
Rootfs暂存安装                  PASS
ARM Runtime Stability           PASS
ASan/LSan/UBSan                 PASS
Job Control真机验证             PASS
```

V1.6已经完成MiniShell从PC/Linux用户态工程向真实ARM Linux板端环境的第一次完整迁移
后续设备接口以及Device Layer进入下一版本继续开发

