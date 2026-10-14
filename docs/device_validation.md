# Device Validation文件说明

## 本文件用以记录V1.8 Device Runtime软件验证范围以及后续真实设备验收步骤

## 当前验证状态

当前已经通过SSH连接Orange Pi 5 Plus.

本次硬件环境为开发板以及网线,尚未连接STM32、OLED或者外部Button.

当前板端已经确认:

```text
Ubuntu 20.04
Linux 6.1.43
aarch64
/dev/ttyS3
/dev/i2c-4
/sys/class/leds/blue_led
/sys/class/leds/green_led
```

设备节点存在只能说明Linux提供相应接口,不能证明外设已接线或者通信成功.

本次已经在真实aarch64开发板执行:

```text
Native Build
Unit Test
Integration Test
PTY UART Protocol
Strict Build
Sanitizer
Make / CMake
Package / Install
cppcheck
ARM Runtime Stability
Valgrind
```

完整执行bash scripts/check_release.sh,15项检查全部PASS.

Normal以及Strict构建均完成146组Unit Test以及49组Integration Test,Integration共396 Assertions.

Unit Assertion数量会随板端可读取的硬件信息变化,不把某次Assertion总数作为固定验收条件.

软件检查以及SSH连接不能替代:

```text
Orange Pi GPIO物理输入
Orange Pi I2C OLED显示
Orange Pi UART3与STM32通信
```

STM32通信、OLED实际显示以及Button按下/释放变化需要接入对应外设后执行.

Board LED已经通过本轮device集成命令检查,板端FD也完成重复Open / Close检查.

这些结果来自本轮执行,不会用历史PASS替代.

## 本轮板端结果

Release检查结果:

```text
Normal Build / Unit / Integration   PASS
Strict Build / Unit / Integration   PASS
ASan / LSan / UBSan                 PASS
cppcheck                           PASS
CMake Debug / Release              PASS
ARM64 Native Runtime               PASS
Runtime Smoke / Error Paths        PASS
ARM Runtime Stability              PASS
Package / Install                  PASS
Valgrind                           PASS
Release Check                      15 / 15 PASS
```

额外使用当前/dev/ttyS3检查未接外设时的UART路径:

```text
device open stm32                  PASS
PING Timeout                       约1.002s
Timeout后Device状态                OPEN
Timeout后Shell继续执行命令         PASS
20次UART Open / Close              PASS
FD数量                             5 -> 6 -> 5
打开设备时Reload                   EBUSY
```

超时只说明本次没有收到合法回复,不作为STM32通信成功的证据.

20次Open / Close后FD回到基线,本次循环未发现FD增长;这不是长期连续运行结论.

Runtime Stability检查中FD为5 -> 5,RSS为496KB -> 1380KB,增长884KB,处于原检查阈值内,未发现Zombie并正常退出.

Runtime Stability PASS对应本轮短期检查,不代表多日持续运行验证.

未提权的I2C以及GPIO访问返回EACCES,Shell继续运行;Board LED读取正常.

Board LED通过用户sudo执行后,再通过SSH读取板端日志核对:

```text
device led blue_led on             PASS
device led blue_led off            PASS
device led blue_led trigger heartbeat PASS
20次Open / On / Off / Close        PASS
```

检查完成后blue_led的Trigger恢复为heartbeat.

本轮没有启动Hardware Watchdog.

## 已有Foundation记录

仓库保留此前Bring-up结果:

```text
docs/gpio.md
docs/i2c_oled.md
docs/uart_serial.md
docs/board_runtime_hardware.md
docs/orangepi5plus.md
```

这些记录确认了当时独立Hardware Tool以及板级接口的工作情况.

不会将历史PASS自动转换为本次Device Runtime或者STM32协议PASS.

Watchdog保持原有独立工具,不属于本次Device命令验收.

## Unit Test范围

Device以及DeviceManager检查:

```text
参数检查
初始化
重复设备名
设备数量上限
配置登记不打开硬件
显式Open后允许访问
重复Open / Close
Disabled设备
Backend错误传播
设备关闭以及Context释放
配置事务加载
打开设备时拒绝Reload
```

设备配置检查:

```text
每种设备的必需字段
非法类型
非法数字
非法波特率
非法I2C地址
非法GPIO方向以及Bias
重复Section / Key
过长Name / Path
缺失配置
配置失败保留原集合
```

实际用例以当前测试源码注册内容为准.

## PTY UART验证

PTY作为Linux软件UART测试端.

```text
Device UART Backend
 |
SerialPort
 |
PTY Slave
 |
PTY Master
 |
模拟STM32回复
```

协议检查:

```text
PING / PONG
STATUS
LED ON / OFF
HEARTBEAT
LF / CRLF
分段Reply
整体Timeout
错误Reply
过长Reply
设备关闭
```

PTY可以验证字节流以及协议状态,不能验证RK3588 UART引脚或者STM32固件.

## Shell命令检查

```text
device list
device info <name>
device open <name>
device close <name>
device reload
device read <name> [count]
device write <name> <byte ...>
```

错误路径要求:

```text
非法参数返回非0
未知设备返回非0
缺失设备节点返回非0
错误类型操作返回非0
错误配置不替换原设备集合
Shell本身继续运行
后续普通命令可以执行
```

device属于Builtin,继续遵守当前Builtin不支持Pipeline以及后台执行的规则.

## 板端准备

先核实目标开发板实际接口:

```bash
ls -l /dev/ttyS3
ls -l /dev/i2c-4
ls -l /dev/gpiochip1
ls -l /sys/class/leds/blue_led
```

默认配置来自此前Orange Pi 5 Plus记录:

```text
UART  : /dev/ttyS3,115200 8N1
OLED  : /dev/i2c-4,0x3C
Button: /dev/gpiochip1,Line 30,Pull-up
LED   : blue_led
```

其他开发板需要修改:

```text
config/devices.conf
```

GPIO、I2C以及UART访问需要当前Linux用户拥有对应设备权限.

LED写入同样遵守sysfs权限.

本轮检查时GPIO设备节点由root持有,权限为0600;LED属性由root持有,权限为0644.

当前orangepi用户没有免密sudo,需要写LED时由用户sudo执行;不能把未提权时的设备权限错误记为驱动或者Device Layer故障.

需要管理员权限的验收使用有权限的会话执行,不通过全局chmod修改设备权限.

## 手册接线核对

以下定义来自用户提供的OrangePi_5_Plus_RK3588_用户手册_v2.1.pdf.

PDF页码从文件第一页开始计算,书页以页面底部印刷页码为准.

```text
用途       40Pin物理引脚        功能             配置
UART TX    16                  UART3_M1 TX      uart3-m1
UART RX    18                  UART3_M1 RX      uart3-m1
OLED SDA   22                  I2C4_M3 SDA      i2c4-m3
OLED SCL   32                  I2C4_M3 SCL      i2c4-m3
Button     7                   GPIO1_D6         Input / Pull-up
Button GND 9                   GND
```

UART表位于PDF第234页/书页223,I2C表位于PDF第231页/书页220.

GPIO1_D6对应Physical Pin 7以及wPi 2,见PDF第225页/书页214;Input与上下拉示例见PDF第227页/书页216.

GPIO1_D6的Bank内Line Offset为30,wPi 2不是GPIO Character Device的Line Offset.

本轮已经核对当前板端:

```text
/dev/gpiochip1 -> fec20000.gpio
/sys/class/gpio/gpiochip32/label = gpio1
base = 32
ngpio = 32
```

因此当前/dev/gpiochip1,Line 30与GPIO1_D6对应.

其他镜像仍然需要核对实际gpiochip与gpio1 Label,不能仅凭Physical Pin或者wPi编号填写.

PDF第223页/书页212说明40Pin GPIO电平为3.3V.

STM32 TX接Orange Pi RX,STM32 RX接Orange Pi TX,并连接GND.

OLED地址0x3C来自已有模块配置,不是手册对所有I2C设备的固定地址.

当前没有外接设备,以上是后续接线依据,不是本轮已完成接线记录.

## Overlay以及引脚复用

手册Ubuntu/Debian章节通过以下路径配置40Pin接口:

```text
sudo orangepi-config
 |
System
 |
Hardware
 |
uart3-m1 / i2c4-m3
 |
Save
 |
Back
 |
Reboot
```

I2C启用步骤见PDF第231到233页/书页220到222,UART步骤见PDF第234到236页/书页223到225.

手册对该镜像说明UART及I2C默认关闭,当前板已经存在对应设备节点,不因手册说明而重复修改启动配置或者重启.

同一物理引脚不能同时用于多种复用功能:

```text
Pin 7     GPIO1_D6 / I2C8_M2 SCL / UART1 RTS / PWM14
Pin 16/18 UART3_M1 / CAN1 / PWM12及PWM13
Pin 22/32 I2C4_M3 / SPI4_M2 / PWM
```

当前默认选择UART3_M1、I2C4_M3以及GPIO1_D6,后续启用其他Overlay以前先检查实际引脚占用.

Board LED定义见PDF第160页/书页149:

```text
/sys/class/leds/blue_led
/sys/class/leds/green_led
```

红色LED为硬件上电指示,不作为软件控制目标.

## STM32连接信息

正式UART实物验收以前需要确认:

```text
STM32具体型号以及开发板
使用的UART / USART
STM32 TX / RX引脚
STM32 LED对应引脚
当前固件入口以及构建方式
Orange Pi实际设备节点
双方串口参数
```

当前仓库协议示例不默认某一款STM32芯片或者GPIO映射.

Orange Pi与STM32 TX/RX交叉连接并共地,双方使用兼容的3.3V串口逻辑.

此前USB-TTL双向PASS只证明当时UART物理链路,不证明STM32已经实现当前协议.

## STM32协议验收

在MiniShell中执行:

```text
device info stm32
device open stm32
device ping stm32
device status stm32
device led stm32 on
device status stm32
device led stm32 off
device status stm32
device heartbeat stm32 42
```

要求:

```text
PING返回PONG
STATUS包含合法LED以及UPTIME_MS
LED ON返回OK LED=1且实际LED亮
LED OFF返回OK LED=0且实际LED灭
STATUS与实际LED状态一致
UPTIME_MS随时间增加
HEARTBEAT回包数值一致
```

再断开STM32通信链路执行PING.

要求请求在Timeout后失败,MiniShell仍能执行:

```text
pwd
status
```

超时后先关闭stm32,确认STM32已经排空、重启或者等待其最大处理时间,再重新打开并重复PING以及STATUS.

当前协议没有完整Transaction ID,flush不能清除未来才到达的旧回复.

LED命令超时以后不要自动重发,恢复通信后先STATUS确认实际状态.

本次STM32一问一答验收不启用非请求自发上报.

## GPIO验收

按上文完成Button接线并核对引脚以后执行:

```text
device info button
device open button
device read button
```

要求:

```text
Released = 1
Pressed  = 0
Released后恢复1
```

Input Button执行write时应返回失败.

Button验收接线为GPIO到GND,不在该连接下验证GPIO Output.

## OLED验收

```text
device info oled
device open oled
device oled oled clear
device oled oled text 0 0 Hello Device
device oled oled refresh
device oled oled off
device oled oled on
```

要求:

```text
显示文本内容正确
clear以后Framebuffer清空
refresh以后屏幕更新
off以后关闭显示
on以后恢复显示
```

中文测试只能使用当前字模已经包含的字符.

## Board LED验收

本轮已执行以下Device集成命令并核对板端日志,命令返回与sysfs Brightness / Trigger回读结果PASS.

```text
device info blue_led
device open blue_led
device led blue_led on
device read blue_led
device led blue_led off
device read blue_led
device led blue_led trigger heartbeat
```

要求:

```text
on后Brightness=max
off后Brightness=0
on/off后Trigger=none
heartbeat后Active Trigger=heartbeat
```

实际LED亮灭现象可以在板端按以上命令人工复核.

本轮额外完成20次Open / On / Off / Close循环,结束后将Trigger恢复为heartbeat.

## Reload以及退出验收

设备打开时执行:

```text
device reload
```

要求拒绝Reload,原有设备保持可用.

关闭默认设备以后执行:

```text
device close stm32
device close oled
device close button
device close blue_led
cd /tmp
device reload
device list
```

要求Reload继续读取启动时确定的配置文件.

最后退出MiniShell,重新启动以后可以再次打开这些设备.

检查过程中记录:

```text
Build Commit
Kernel / Architecture
实际devices.conf
接线
命令输出
实际硬件现象
FD基线以及结束数量
错误恢复
退出结果
```

## V1.8完成条件

软件检查与真实设备验收分别记录.

STM32、OLED、GPIO以及LED新集成全部完成实物验收以后,再更新对应板端记录以及Issue状态.

当前不会将V1.8全验收标记为Done.
