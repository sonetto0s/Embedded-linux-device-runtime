# Board Runtime Hardware文件说明

## 本文件用以记录Orange Pi 5 Plus板级Runtime Hardware验证情况

当前Phase主要验证:

```text
Board LED
Thermal Sensor
Hardware Watchdog
```

当前PWM Fan暂不进行真实硬件验证.

## Board LED

Orange Pi 5 Plus当前可以看到:

```text
/sys/class/leds/green_led
/sys/class/leds/blue_led
/sys/class/leds/mmc0::
```

当前板载状态灯主要为:

```text
green_led
blue_led
```

当前green_led以及blue_led默认Trigger为:

```text
heartbeat
```

当前已经通过MiniShell现有LED命令完成真实开发板验证:

```text
led list
led info green_led
led on green_led
led off green_led
led trigger green_led heartbeat
```

当前green_led以及blue_led均可以通过Linux LED sysfs接口进行控制.

当前LED控制继续复用:

```text
src/led_control.c
```

Phase 4不会重新建立新的LED控制模块.

## Thermal Sensor

当前Orange Pi 5 Plus可以发现7个Thermal Zone:

```text
thermal_zone0
thermal_zone1
thermal_zone2
thermal_zone3
thermal_zone4
thermal_zone5
thermal_zone6
```

当前对应类型:

```text
soc-thermal
bigcore0-thermal
bigcore1-thermal
littlecore-thermal
center-thermal
gpu-thermal
npu-thermal
```

当前温度通过:

```text
/sys/class/thermal/thermal_zone*/temp
```

读取.

sysfs中的温度值使用毫摄氏度表示.

例如:

```text
31461
```

表示:

```text
31.461 C
```

当前系统:

```bash
sensors
```

可以正常读取RK3588各组Thermal Sensor.

当前真实开发板测试时各组温度约为:

```text
30 C - 32 C
```

当前MiniShell:

```text
monitor
```

可以正常读取Runtime Thermal信息.

真实测试结果:

```text
Temperature : 31.5 C (soc-thermal)
```

当前结果与:

```text
sensors
/sys/class/thermal/thermal_zone*/temp
```

读取结果一致.

当前Thermal Runtime继续复用:

```text
src/thermal_monitor.c
```

Phase 4不会重新建立新的Temperature模块.

## Hardware Watchdog

当前Orange Pi 5 Plus存在:

```text
/dev/watchdog
/dev/watchdog0
```

当前真实Watchdog设备:

```text
/dev/watchdog0
```

当前设备节点Major以及Minor:

```text
MAJOR=243
MINOR=0
```

当前sysfs设备路径:

```text
/sys/devices/platform/feaf0000.watchdog/watchdog/watchdog0
```

当前Platform Device:

```text
feaf0000.watchdog
```

当前Linux Driver:

```text
dw_wdt
```

当前Device Tree Node:

```text
/watchdog@feaf0000
```

当前Device Tree Compatible:

```text
snps,dw-wdt
```

因此当前Watchdog硬件访问路径:

```text
RK3588
 |
watchdog@feaf0000
 |
snps,dw-wdt
 |
dw_wdt
 |
Linux Watchdog Core
 |
/dev/watchdog0
```

## Kernel Configuration

当前Linux Kernel配置:

```text
CONFIG_WATCHDOG=y
CONFIG_WATCHDOG_CORE=y
CONFIG_DW_WATCHDOG=y
```

当前:

```text
CONFIG_WATCHDOG_NOWAYOUT
```

没有启用.

当前:

```text
CONFIG_WATCHDOG_SYSFS
```

没有启用.

因此当前:

```text
/sys/class/watchdog/watchdog0
```

只提供基础设备信息.

当前不会提供:

```text
identity
timeout
min_timeout
max_timeout
nowayout
```

等Watchdog扩展sysfs属性.

因此当前Watchdog Runtime信息主要通过:

```text
Linux Watchdog ioctl API
```

获取.

## WatchdogDevice

当前新增:

```text
include/watchdog_device.h
src/watchdog_device.c
```

当前WatchdogDevice保存:

```text
fd
watchdog_info
timeout
has_info
```

当前主要接口:

```text
watchdog_device_init
watchdog_device_open
watchdog_device_get_timeout
watchdog_device_set_timeout
watchdog_device_keepalive
watchdog_device_get_timeleft
watchdog_device_disable
watchdog_device_close
```

当前WatchdogDevice负责Linux Watchdog字符设备访问.

当前不会直接访问RK3588 Watchdog寄存器.

## Linux Watchdog API

当前通过:

```text
open
ioctl
write
close
```

访问Linux Watchdog设备.

当前主要使用:

```text
WDIOC_GETSUPPORT
WDIOC_GETTIMEOUT
WDIOC_SETTIMEOUT
WDIOC_KEEPALIVE
WDIOC_GETTIMELEFT
WDIOC_SETOPTIONS
```

当前Watchdog能力通过:

```text
struct watchdog_info
```

读取.

## 当前Watchdog能力

真实开发板查询结果:

```text
Identity      : Synopsys DesignWare Watchdog
Firmware      : 0
Timeout       : 44 s
Options       : 0x00008380
```

当前Capabilities:

```text
SETTIMEOUT : yes
MAGICCLOSE : yes
KEEPALIVE  : yes
PRETIMEOUT : yes
```

因此当前Driver支持:

```text
Timeout配置
Magic Close
Keepalive
Pretimeout
```

## Timeout

当前真实测试请求:

```text
10 s
```

Driver实际返回:

```text
11 s
```

因此当前不会假设:

```text
Requested Timeout
```

一定等于:

```text
Actual Timeout
```

当前应用层以:

```text
WDIOC_SETTIMEOUT
```

返回后的实际Timeout为准.

## Keepalive

当前已经完成真实Hardware Watchdog Keepalive测试.

测试命令:

```bash
sudo ./build/default/watchdog_demo \
    /dev/watchdog0 \
    keepalive \
    10 \
    15
```

当前实际Timeout:

```text
11 s
```

当前程序每秒调用一次:

```text
WDIOC_KEEPALIVE
```

真实测试持续:

```text
15 s
```

测试过程中Time Left保持正常刷新.

当前Hardware Watchdog没有触发系统Reset.

测试结果:

```text
Hardware Watchdog Keepalive PASS
```

## Magic Close

当前Watchdog Capability包含:

```text
WDIOF_MAGICCLOSE
```

因此WatchdogDevice正常关闭前会向Watchdog设备写入:

```text
V
```

然后关闭Watchdog文件描述符.

当前用于通知Linux Watchdog Core:

```text
程序正在正常关闭Watchdog
```

当前不会简单执行:

```text
close
```

然后假设Watchdog一定停止.

## Signal退出

当前watchdog_demo处理:

```text
SIGINT
SIGTERM
```

运行:

```bash
sudo ./build/default/watchdog_demo \
    /dev/watchdog0 \
    keepalive \
    10 \
    60
```

在运行过程中使用:

```text
Ctrl+C
```

当前程序可以从Keepalive循环退出并进入Watchdog关闭流程.

真实测试已经可以正常返回Shell.

最终Phase 4测试还会在退出后等待超过Actual Timeout时间确认Watchdog已经停止.

## watchdog_demo

当前新增:

```text
tools/watchdog_demo.c
```

当前支持:

```text
info
keepalive
```

查询Watchdog:

```bash
sudo ./build/default/watchdog_demo \
    /dev/watchdog0 \
    info
```

Keepalive测试:

```bash
sudo ./build/default/watchdog_demo \
    /dev/watchdog0 \
    keepalive \
    10 \
    15
```

当前不提供默认的Watchdog Reset测试模式.

Phase 4重点验证:

```text
Watchdog API
Capabilities
Timeout
Keepalive
Magic Close
Signal Exit
Safe Close
```

当前不会为了验证Watchdog而反复触发开发板重启.

## Make集成

当前新增:

```bash
make watchdog-demo
```

当前生成:

```text
build/default/watchdog_demo
```

watchdog_demo主要编译:

```text
tools/watchdog_demo.c
src/watchdog_device.c
```

当前:

```bash
make check
```

会构建:

```text
gpio_probe
i2c_probe
oled_demo
serial_test
watchdog_demo
```

当前:

```bash
make strict
```

会使用:

```text
-Werror
```

完成测试以及全部Hardware Tool构建.

当前:

```text
src/watchdog_device.c
```

没有加入:

```text
APP_SRC
```

当前Watchdog仍然属于Board Runtime Hardware Foundation.

## CMake集成

CMake当前新增:

```text
watchdog_demo
```

Target.

当前watchdog_demo主要编译:

```text
tools/watchdog_demo.c
src/watchdog_device.c
```

当前没有将:

```text
src/watchdog_device.c
```

加入:

```text
MINISHELL_CORE_SOURCES
```

