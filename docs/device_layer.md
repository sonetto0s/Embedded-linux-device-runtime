# Device Layer文件说明

## 本文件用以记录V1.8 Device Runtime接口,设备生命周期以及MiniShell设备命令

## 当前实现范围

当前Device Layer统一管理:

```text
UART
I2C
OLED
GPIO
Board LED
```

当前使用同步设备访问以及UART一问一答协议.

EventLoop、epoll、GPIO Edge以及自动Heartbeat调度留到V1.9.

Watchdog继续保持独立Hardware Foundation工具,本次不会自动打开或者启动Hardware Watchdog.

## 整体流程

```text
device Command
 |
cmd_device
 |
DeviceManager
 |
Device
 |
DeviceOps
 |
UART / I2C / OLED / GPIO / LED Backend
 |
已有Linux硬件访问层
 |
Linux Kernel
 |
实际设备
```

Device Layer不直接访问RK3588寄存器.

已有Bring-up工具继续保留,用于单独检查底层物理链路.

## Device

文件:

```text
include/device.h
src/device.c
```

当前Device保存:

```text
DeviceConfig config
const DeviceOps *ops
context
state
opened
last_errno
```

context由对应Backend保存底层设备状态.

last_errno用于保留最近一次设备操作错误.

## DeviceOps

当前接口:

```text
open
read
write
control
close
destroy
```

公共Device接口负责参数以及生命周期检查.

Backend负责实际Linux设备访问.

不支持的设备操作返回错误,不会静默当作成功.

open以及close成功返回0,失败返回-1并设置errno.

control通常成功返回0,失败返回-1并设置errno.

UART_WAIT返回1表示可读,0表示等待超时,-1表示失败.

read以及write成功返回实际Byte数量,失败返回-1并设置errno.

Backend Context在登记成功后由DeviceManager管理,关闭FD与释放Context是两个步骤.

## Device状态

```text
DEVICE_CLOSED
DEVICE_OPEN
DEVICE_ERROR
DEVICE_DISABLED
```

设备配置加载以后不会立即打开硬件.

需要实际访问设备时先执行device open,不会因为read、write或者协议请求自动打开硬件.

因此缺失UART、I2C或者GPIO设备节点不会影响Shell启动以及其他普通命令.

设备操作失败以后可以通过:

```text
device info <name>
device close <name>
device open <name>
```

检查错误并重新打开设备.

正常OPEN设备重复open不会重复申请硬件.

仍然opened但状态为ERROR的设备需要先close,再次open不会绕过恢复流程.

close可以重复执行.

enabled=0的设备保持DISABLED,open返回失败并且不访问硬件.

## DeviceManager

文件:

```text
include/device_manager.h
src/device_manager.c
```

当前DeviceManager保存:

```text
Device devices[DEVICE_MAX_COUNT]
count
```

当前最大设备数量:

```text
16
```

设备名最多63 Bytes,设备路径最多255 Bytes.

主要接口:

```text
device_manager_init
device_manager_register
device_manager_find
device_manager_get
device_manager_load
device_manager_close_all
device_manager_destroy
```

设备通过Name查找,同一份配置中不会允许重复设备名.

DeviceManager由ShellContext统一管理.

Shell退出时关闭设备并释放Backend Context.

## Device Backend

当前Backend复用:

```text
UART   -> serial_port
I2C    -> i2c_bus
OLED   -> oled + oled_font + i2c_bus
GPIO   -> gpio_line
LED    -> led_control + sysfs_io
```

当前GPIO使用Linux GPIO Character Device.

LED继续使用Linux LED sysfs,不会额外实现寄存器控制.

OLED使用Framebuffer,绘制以后通过refresh发送到设备.

公共read/write Buffer:

```text
UART / I2C -> 字节数组
GPIO / LED -> int,Buffer Size=sizeof(int)
OLED       -> 不支持通用read/write,使用control
```

主要control参数:

```text
UART_WAIT      -> int Timeout指针
UART_WRITE_ALL -> DeviceWriteRequest指针
UART_FLUSH     -> NULL
OLED_TEXT      -> DeviceText指针
LED_TRIGGER    -> Trigger字符串
```

OLED clear、refresh、on/off以及LED on/off不需要额外参数.

## 设备配置

设备配置文件:

```text
config/devices.conf
```

设备配置与原有Shell配置分开保存.

```text
config/config.conf   -> Prompt / max_job / debug
config/devices.conf  -> Device参数
```

每个INI Section表示一台设备:

```text
[stm32]
type=uart
path=/dev/ttyS3
baudrate=115200
enabled=1
```

所有设备必需:

```text
[设备名]
type
```

设备名只使用字母、数字、下划线以及连字符.

公共可选参数:

```text
enabled=1
```

设置enabled=0时登记设备但不允许打开.

每种设备的必需字段:

```text
uart : path
i2c  : path,address
oled : path,address
gpio : path,offset
led  : led_name
```

path必须为绝对路径.

UART可选参数以及默认值:

```text
baudrate=115200
```

当前支持9600、19200、38400、57600、115200以及230400.

230400还需要目标平台termios提供B230400,以实际open结果为准.

GPIO可选参数以及默认值:

```text
direction=input
```

direction支持input以及output.

Input GPIO的bias默认default,支持:

```text
default
pull_up
pull_down
disabled
```

Output GPIO可以额外设置:

```text
initial_value=0
```

initial_value默认0,只接受0或者1,Input配置不接受此字段.

Output配置不接受bias,避免将未实际应用的Bias写成已经生效.

I2C以及OLED的address范围:

```text
0x03 - 0x77
```

数字使用base0解析,可以使用十进制或者0x十六进制形式.

前导0按八进制处理,普通十进制数不额外补前导0.

重复Section、重复Key、未知Key、类型不相关字段以及非法值均使整次加载失败.

当前只接受整行#或者;注释,参数值以后不使用行尾注释.

完整默认设备配置以:

```text
config/devices.conf
```

为准.

默认设备名:

```text
stm32
oled
button
blue_led
```

默认路径来自仓库已有Orange Pi 5 Plus Bring-up记录.

这些值不是所有开发板的通用映射,部署以前需要核实实际Device Node、引脚以及接线.

## Orange Pi 5 Plus配置依据

用户提供的OrangePi_5_Plus_RK3588_用户手册_v2.1.pdf中:

```text
UART3_M1 : TX Pin 16,RX Pin 18
I2C4_M3  : SDA Pin 22,SCL Pin 32
GPIO1_D6 : Pin 7,wPi 2,Bank内Line Offset 30
Board LED: blue_led
```

UART定义位于PDF第234页/书页223,I2C定义位于PDF第231页/书页220.

GPIO定义位于PDF第225页/书页214,上下拉示例位于PDF第227页/书页216.

LED sysfs定义位于PDF第160页/书页149.

wPi编号不能直接作为GPIO Character Device的offset,gpiochip编号也需要在目标板核对gpio1 Label.

本轮已确认gpiochip1对应fec20000.gpio,gpio1的base=32、ngpio=32,因此当前Line 30对应GPIO1_D6.

UART3以及I2C4对应Overlay为uart3-m1以及i2c4-m3,设备节点已存在时先检查当前状态,不重复修改启动配置.

GPIO Pin 7与I2C8_M2 SCL、UART1 RTS以及PWM14复用;UART Pin 16/18与CAN1及PWM复用;I2C Pin 22/32与SPI4_M2及PWM复用.

启用额外Overlay以前需要确认这些引脚没有冲突.

本轮已经通过SSH连接Ubuntu 20.04、Linux 6.1.43的aarch64开发板,确认ttyS3、i2c-4以及blue_led接口存在.

当前没有连接STM32、OLED或者外部Button,节点存在不代表外设通信已完成.

完整接线、权限以及本轮验证范围见docs/device_validation.md.

## 配置查找

当前查找顺序:

```text
MINISHELL_DEVICES_CONFIG
 |
<exe>/../config/devices.conf
 |
<exe>/config/devices.conf
 |
<cwd>/config/devices.conf
```

Shell启动时保存稳定Device Config路径.

因此:

```text
cd /tmp
device reload
```

仍然使用启动时确定的设备配置来源.

设备配置不存在时Shell仍然可以启动.

```text
device list
```

可以查看当前登记设备.

配置重新加载时只在完整解析以及Backend准备成功后替换原有设备集合.

当前存在已经打开的设备时拒绝reload,需要先关闭设备.

仍然opened的ERROR设备同样会阻止reload.

## 基础设备命令

```text
device list
device info <name>
device open <name>
device close <name>
device reload
```

示例:

```text
device list
device info stm32
device open stm32
device close stm32
device reload
```

list以及info只查看管理状态,不会为了显示列表提前打开所有硬件.

## Read以及Write

```text
device read <name> [count]
device write <name> <byte ...>
```

UART以及I2C read使用十六进制显示原始字节.

count默认16,接受1到256,表示本次最多读取的Byte数量.

UART read等待最多1000ms,返回当前收到的字节,不会要求一次读取填满count.

write参数使用整数形式,例如:

```text
device open stm32
device write stm32 0x50 0x49 0x4e 0x47 0x0a
```

每个UART或者I2C数据参数必须在一个Byte范围内.

GPIO以及LED read/write使用:

```text
0
1
```

Input GPIO不能执行write.

当前Button配置是Input,不应在Button与GND保持连接时改为Output.

## STM32 UART协议

```text
device ping <uart>
device status <uart>
device led <uart> on
device led <uart> off
device heartbeat <uart> <uint>
```

默认设备示例:

```text
device open stm32
device ping stm32
device status stm32
device led stm32 on
device led stm32 off
device heartbeat stm32 42
```

当前Request以及Reply:

```text
PING
PONG

STATUS
STATUS LED=<0|1> UPTIME_MS=<uint32>

LED ON
OK LED=1

LED OFF
OK LED=0

HEARTBEAT <uint32>
OK HEARTBEAT=<相同数值>
```

Request自动追加LF.

Reply允许LF或者CRLF结尾.

当前一行Payload最大长度:

```text
127 bytes
```

不包含结尾CR、LF以及字符串NUL.

当前协议Request使用固定整体Timeout:

```text
1000 ms
```

发送以及接收共享同一Deadline.

该Timeout只覆盖输入清理、发送以及接收,不包含此前显式open所需时间.

Timeout、错误Reply以及过长Reply均返回失败,Shell继续运行.

每次Request以前清除串口旧输入,当前接口用于一问一答设备,不会同时接收独立遥测数据流.

HEARTBEAT仅回显本次uint32数值,不会设置发送周期或者启动定时任务.

HEARTBEAT参数只接受十进制数字,范围为0到4294967295.

PING、STATUS以及LED回复没有Transaction ID,HEARTBEAT只有本次数值回显.

输入flush只能清除已经到达的数据,不能保证清除之后才到达的旧PONG或者STATUS.

发生Timeout后先close,确认STM32已经排空、重启或者等待其最大处理时间,再open并发起新请求.

LED命令Timeout表示结果未确认,不自动重发写操作.恢复通信后先STATUS查询实际状态.

STM32端在当前一问一答模式下不发送非请求的主动上报.

## OLED命令

```text
device oled <oled> clear
device oled <oled> text <x> <y> <words ...>
device oled <oled> refresh
device oled <oled> on
device oled <oled> off
```

示例:

```text
device open oled
device oled oled clear
device oled oled text 0 0 Hello Device
device oled oled refresh
```

text参数中的多个Word会组成显示文本.

当前Parser不提供引号解析,示例不需要额外添加双引号.

新增中文显示内容仍然需要已有OLED字模包含对应字符.

## Board LED命令

```text
device led <LEDname> on
device led <LEDname> off
device led <LEDname> trigger <trigger>
```

示例:

```text
device open blue_led
device led blue_led on
device led blue_led off
device led blue_led trigger heartbeat
```

UART设备的device led命令发送STM32协议.

LED设备的device led命令使用本机Linux LED subsystem.

原有:

```text
led list
led info blue_led
led on blue_led
```

继续保留.

## 当前验证范围

本次V1.8已经在Orange Pi 5 Plus完成完整Release检查,15项全部PASS.

Normal以及Strict均完成146组Unit Test与49组Integration Test,并通过Sanitizer、cppcheck、CMake、ARM Runtime Stability、Package / Install以及Valgrind检查.

UART协议自动测试使用PTY模拟设备端.

已有GPIO、OLED、UART以及Watchdog真机记录属于Hardware Foundation阶段.

本轮板载LED的device on/off/heartbeat命令以及20次Open / On / Off / Close检查PASS,结束后恢复heartbeat.

UART未接外设时PING在约1.002s返回Timeout,后续Shell命令正常,20次Open / Close的FD回到基线.

软件检查、板载LED控制以及外接设备验收分别记录,本轮没有启动Hardware Watchdog.

STM32通信、OLED显示以及Button输入仍然需要接入实际外设后复验.

详细验收步骤:

```text
docs/device_validation.md
```
