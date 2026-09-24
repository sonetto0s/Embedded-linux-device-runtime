# UART Serial文件说明

## 本文件用以记录Orange Pi 5 Plus UART3接口Bring-up以及当前SerialPort访问层实现情况

## 当前UART环境

当前Orange Pi 5 Plus通过40Pin使用:

```text
UART3_M1
```

当前Linux设备节点:

```text
/dev/ttyS3
```

当前串口配置:

```text
115200 baud
8 data bits
No parity
1 stop bit
No flow control
Raw mode
```

即:

```text
115200 8N1
```

## UART3引脚

当前UART3_M1使用Orange Pi 5 Plus 40Pin中的:

```text
Physical Pin 16
Physical Pin 18
```

对应:

```text
Physical Pin 16 = UART3 TX
Physical Pin 18 = UART3 RX
```

当前gpio readall中两组引脚均处于:

```text
ALT10
```

当前UART3连接关系:

```text
Orange Pi Physical Pin 16
 |
UART3 TX
 |
External UART RX
```

```text
Orange Pi Physical Pin 18
 |
UART3 RX
 |
External UART TX
```

UART通信使用3.3V逻辑电平.

当前不会将UART3 RX以及TX直接连接到5V逻辑信号.

## UART3启用

当前通过:

```bash
sudo orangepi-config
```

进入:

```text
System
 |
Hardware
 |
uart3-m1
```

启用UART3_M1.

重启以后确认:

```bash
ls -l /dev/ttyS3
```

当前设备节点正常存在.

执行:

```bash
gpio readall
```

当前Physical Pin 16以及Physical Pin 18均处于UART复用状态.

## UART设备占用检查

当前通过:

```bash
sudo fuser -v /dev/ttyS3
```

检查UART3是否被其他进程占用.

当前无其他进程占用UART3设备节点.

当前同时检查:

```bash
systemctl status serial-getty@ttyS3.service --no-pager
```

当前结果:

```text
inactive (dead)
```

因此当前UART3没有被serial-getty占用.

## UART3硬件回环

当前已经完成UART3_M1硬件回环验证.

测试连接:

```text
Physical Pin 16 UART3 TX
 |
Physical Pin 18 UART3 RX
```

即直接将UART3 TX以及UART3 RX短接.

执行:

```bash
sudo gpio serial /dev/ttyS3
```

当前可以连续看到:

```text
Out:   0:  ->   0
Out:   1:  ->   1
Out:   2:  ->   2
Out:   3:  ->   3
```

发送数据与接收数据一致.

因此当前已经验证:

```text
UART3 Controller
/dev/ttyS3
UART3 TX
UART3 RX
Physical Pin 16
Physical Pin 18
```

均可以正常工作.

## Linux用户态UART回环

当前也通过Linux用户态设备文件完成UART3回环验证.

首先配置:

```bash
sudo stty -F /dev/ttyS3 \
    115200 \
    cs8 \
    -cstopb \
    -parenb \
    -crtscts \
    -ixon \
    -ixoff \
    raw \
    -echo
```

当前配置确认:

```bash
sudo stty -F /dev/ttyS3 -a
```

当前可以看到:

```text
speed 115200 baud
cs8
-parenb
-cstopb
-crtscts
-ixon
-ixoff
```

在UART3 TX以及RX短接状态下发送:

```text
OrangePi UART3 LOOPBACK PASS
```

当前UART3可以正常接收相同数据.

因此当前Linux TTY用户态访问路径正常.

## USB-TTL验证

当前使用USB-TTL作为UART外部测试端.

USB-TTL首先完成独立回环测试.

测试连接:

```text
USB-TTL TXD
 |
USB-TTL RXD
```

Windows串口端发送数据以后可以正常接收到相同数据.

因此当前USB-TTL TX以及RX功能正常.

## Orange Pi与USB-TTL连接

当前Orange Pi UART3与USB-TTL正式连接:

```text
Orange Pi Physical Pin 16 UART3 TX
 |
USB-TTL RXD
```

```text
Orange Pi Physical Pin 18 UART3 RX
 |
USB-TTL TXD
```

```text
Orange Pi GND
 |
USB-TTL GND
```

TX以及RX采用交叉连接:

```text
TX -> RX
RX <- TX
```

双方共地.

当前USB-TTL通过Windows USB接口供电.

当前不会通过USB-TTL为Orange Pi供电.

## 跨设备UART验证

当前已经完成:

```text
Orange Pi UART3
 |
USB-TTL
 |
Windows Serial Port
```

双向UART通信验证.

当前Orange Pi向Windows发送数据正常.

当前Windows通过USB-TTL向Orange Pi发送数据正常.

因此当前结果:

```text
Orange Pi -> Windows PASS
Windows -> Orange Pi PASS
```

当前UART3以及USB-TTL真实物理通信链路正常.

## SerialPort访问层

当前新增:

```text
include/serial_port.h
src/serial_port.c
```

当前SerialPort Context保存:

```text
fd
```

当前主要接口:

```text
serial_port_init
serial_port_open
serial_port_read
serial_port_write
serial_port_write_all
serial_port_wait_readable
serial_port_flush_input
serial_port_close
```

当前UART访问流程:

```text
Application
 |
SerialPort
 |
termios
 |
Linux TTY
 |
/dev/ttyS3
 |
RK3588 UART3
 |
40Pin UART3_M1
 |
External UART Device
```

当前不会直接访问RK3588 UART控制器寄存器.

## SerialPort打开

当前通过:

```text
open
```

打开UART设备节点.

当前基础打开参数:

```text
O_RDWR
O_NOCTTY
O_CLOEXEC
```

Nonblocking模式额外使用:

```text
O_NONBLOCK
```

其中:

```text
O_NOCTTY
```

用于避免UART成为当前进程控制终端.

```text
O_CLOEXEC
```

用于避免UART文件描述符在exec以后意外继承.

## UART配置

当前通过:

```text
tcgetattr
tcsetattr
```

配置UART.

当前输入以及输出波特率通过:

```text
cfsetispeed
cfsetospeed
```

设置.

当前支持:

```text
9600
19200
38400
57600
115200
230400
```

其中230400在当前系统termios提供B230400时启用.

当前UART配置为:

```text
8 data bits
No parity
1 stop bit
CREAD
CLOCAL
No RTS/CTS
No XON/XOFF
Raw input
Raw output
```

## Raw Mode

当前SerialPort关闭:

```text
Canonical Mode
Echo
Signal Processing
Input CR/LF Translation
Output Post Processing
Software Flow Control
```

因此当前UART按原始字节流进行处理.

当前不会将UART访问层限制为字符串接口.

## Blocking以及Nonblocking

当前SerialPort支持:

```text
Blocking
Nonblocking
```

两种打开模式.

Nonblocking模式使用:

```text
O_NONBLOCK
```

当前无数据可以读取时:

```text
read
```

可能返回:

```text
EAGAIN
EWOULDBLOCK
```

当前不会将EAGAIN以及EWOULDBLOCK视为UART设备故障.

## EINTR处理

当前:

```text
read
write
poll
```

相关路径均考虑:

```text
EINTR
```

系统调用被信号打断时不会直接将其作为UART通信故障.

当前会重新执行相应系统调用或者继续等待.

## Partial Write

UART属于字节流设备.

当前不会假设:

```text
write
```

一次可以发送全部数据.

当前:

```text
serial_port_write_all
```

会持续处理已经发送以及尚未发送的数据.

当前可以处理:

```text
Partial Write
EAGAIN
EWOULDBLOCK
EINTR
Timeout
```

因此当前SerialPort不会依赖单次write完成完整UART数据发送.

## Poll以及Timeout

当前SerialPort使用:

```text
poll
```

等待UART文件描述符进入可读或者可写状态.

当前:

```text
serial_port_wait_readable
```

用于等待UART进入可读状态.

当前:

```text
serial_port_write_all
```

使用整体Deadline进行发送超时控制.

当前不会在Nonblocking模式遇到EAGAIN以后持续Busy Loop.

该实现为后续V1.9 EventLoop以及epoll UART异步事件处理提供基础.

## Serial Test

当前新增UART真实硬件验证工具:

```text
tools/serial_test.c
```

Make构建:

```bash
make serial-test
```

生成:

```text
build/default/serial_test
```

当前支持:

```text
send
recv
```

两种模式.

发送:

```bash
sudo ./build/default/serial_test \
    /dev/ttyS3 \
    115200 \
    send \
    "Hello from SerialPort"
```

Windows串口端可以正常接收发送内容.

接收:

```bash
sudo ./build/default/serial_test \
    /dev/ttyS3 \
    115200 \
    recv
```

Windows通过USB-TTL发送数据以后Orange Pi可以正常接收.

当前serial_test保持为独立Hardware Bring-up工具.

当前没有直接加入MiniShell Runtime Core.

## Build集成

Make当前新增:

```bash
make serial-test
```

当前生成:

```text
build/default/serial_test
```

serial_test主要编译:

```text
tools/serial_test.c
src/serial_port.c
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
```

当前:

```bash
make strict
```

会在:

```text
-Werror
```

条件下完成测试以及Hardware Tool构建.

## CMake集成

CMake当前新增:

```text
serial_test
```

Target.

当前serial_test主要编译:

```text
tools/serial_test.c
src/serial_port.c
```

当前没有将:

```text
src/serial_port.c
```

加入:

```text
MINISHELL_CORE_SOURCES
```

当前UART仍然保持为V1.8 Hardware Bring-up以及Serial Foundation.

正式Device Runtime集成留到后续Device Layer阶段.

当前CMake配置:

```bash
cmake -S . -B build/cmake \
    -DMINISHELL_WARNINGS_AS_ERRORS=ON
```

Build:

```bash
cmake --build build/cmake -j4
```

当前可以构建:

```text
minishell
gpio_probe
i2c_probe
oled_demo
serial_test
minishell_tests
minishell_integration_tests
```

CTest当前运行:

```bash
cd build/cmake
ctest --output-on-failure
```

## 当前UART架构

当前UART部分结构:

```text
serial_test
 |
SerialPort
 |
termios
 |
Linux TTY
 |
/dev/ttyS3
 |
RK3588 UART3
 |
40Pin UART3_M1
 |
External UART Device
```

当前serial_port属于Linux UART基础访问层.

当前没有加入:

```text
STM32 Protocol
PING/PONG
STATUS
LED Control
Heartbeat
DeviceManager
Device
DeviceOps
EventLoop
epoll
UART Async Event
```

这些内容不会提前加入Phase 3.

STM32正式设备通信留到后续Phase.

Event Driven UART留到V1.9.
