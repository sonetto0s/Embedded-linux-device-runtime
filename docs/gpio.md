# GPIO文件说明

## 本文件用以记录Orange Pi 5 Plus GPIO接口Bring-up以及当前GPIO访问层实现情况

## 当前GPIO环境

当前Orange Pi 5 Plus提供:

```text
/dev/gpiochip0
/dev/gpiochip1
/dev/gpiochip2
/dev/gpiochip3
/dev/gpiochip4
/dev/gpiochip5
```

执行:

```bash
sudo gpiodetect
```

当前结果:

```text
gpiochip0 [gpio0] (32 lines)
gpiochip1 [gpio1] (32 lines)
gpiochip2 [gpio2] (32 lines)
gpiochip3 [gpio3] (32 lines)
gpiochip4 [gpio4] (32 lines)
gpiochip5 [rk806-gpio] (3 lines)
```

当前普通用户没有GPIO Character Device访问权限.

GPIO相关测试使用管理员权限执行.

## GPIO测试引脚

当前Phase使用Orange Pi 5 Plus 40Pin中的:

```text
Physical Pin 7
```

对应:

```text
GPIO1_D6
```

wiringOP编号:

```text
2
```

Legacy GPIO编号:

```text
62
```

当前gpio1范围:

```text
32-63
```

因此GPIO1_D6对应gpio1内部Line Offset:

```text
62 - 32 = 30
```

最终Linux GPIO Character Device映射:

```text
/dev/gpiochip1
Line 30
```

当前映射:

```text
Physical Pin 7
 |
GPIO1_D6
 |
GPIO62
 |
gpiochip1
 |
Line 30
```

## Button连接

当前使用普通Button进行GPIO Input验证.

连接:

```text
Orange Pi Physical Pin 7
 |
GPIO1_D6
 |
Button
 |
GND
 |
Orange Pi Physical Pin 9
```

当前不连接:

```text
3.3V
5V
```

GPIO使用内部Pull-up.

因此正常逻辑:

```text
Button Released = 1
Button Pressed  = 0
```

## wiringOP验证

首先通过Orange Pi提供的wiringOP进行GPIO验证.

设置GPIO Input:

```bash
sudo gpio mode 2 in
```

设置内部Pull-up:

```bash
sudo gpio mode 2 up
```

读取:

```bash
sudo gpio read 2
```

当前实机结果:

```text
Released:

1
```

```text
Pressed:

0
```

Button松开以后重新恢复:

```text
1
```

当前Physical Pin 7输入以及内部Pull-up工作正常.

## GPIO Character Device验证

安装GPIO工具:

```bash
sudo apt install gpiod
```

当前版本:

```text
gpiod 1.4.1
```

GPIO Controller确认:

```bash
sudo gpiodetect
```

GPIO Line状态确认:

```bash
sudo gpioinfo
```

当前gpiochip1 Line 30可以作为GPIO Input使用.

直接读取:

```bash
sudo gpioget gpiochip1 30
```

当前结果:

```text
Released:

1
```

```text
Pressed:

0
```

因此已经确认:

```text
Physical Pin 7
 |
GPIO1_D6
 |
/dev/gpiochip1
 |
Line 30
```

映射正常.

## GPIO访问层

当前新增:

```text
include/gpio_line.h
src/gpio_line.c
```

gpio_line通过Linux GPIO Character Device访问GPIO.

当前主要接口:

```text
gpio_line_init
gpio_line_open_input
gpio_line_open_output
gpio_line_get_value
gpio_line_set_value
gpio_line_close
```

当前GPIO访问流程:

```text
Application
 |
gpio_line
 |
Linux GPIO Character Device
 |
/dev/gpiochip*
 |
Rockchip GPIO Driver
 |
RK3588 GPIO
```

当前不会直接操作RK3588 GPIO寄存器.

当前不会依赖wiringOP作为Runtime GPIO访问层.

wiringOP只用于板级Bring-up以及GPIO映射验证.

## GPIO Input

GPIO Input通过:

```text
GPIO_GET_LINEHANDLE_IOCTL
```

申请GPIO Line.

当前支持:

```text
Default Bias
Pull-up
Pull-down
Bias Disabled
```

GPIO值通过:

```text
GPIOHANDLE_GET_LINE_VALUES_IOCTL
```

读取.

当前Button测试使用:

```text
Pull-up
```

## GPIO Output

当前gpio_line已经提供GPIO Output基础能力.

GPIO Output通过:

```text
GPIOHANDLE_REQUEST_OUTPUT
```

申请Line.

GPIO值通过:

```text
GPIOHANDLE_SET_LINE_VALUES_IOCTL
```

设置.

当前Button连接在Physical Pin 7以及GND之间.

因此当前Button实机验证阶段只使用GPIO Input.

不会在Button保持连接时进行GPIO Output测试.

## GPIO Probe

当前新增GPIO硬件验证工具:

```text
tools/gpio_probe.c
```

Make构建:

```bash
make gpio-probe
```

生成:

```text
build/default/gpio_probe
```

单次读取:

```bash
sudo ./build/default/gpio_probe input /dev/gpiochip1 30 up
```

当前结果:

```text
Released:

1
```

```text
Pressed:

0
```

持续读取:

```bash
sudo ./build/default/gpio_probe watch /dev/gpiochip1 30 up
```

当前Button状态变化:

```text
value=1
value=0
value=1
```

其中:

```text
value=1
```

表示Button Released.

```text
value=0
```

表示Button Pressed.

## GPIO Probe Watch

当前watch使用Polling实现.

Polling Interval:

```text
50 ms
```

当前主要用于V1.8 GPIO Bring-up以及真实硬件验证.

当前没有加入:

```text
GPIO Edge Event
epoll
EventLoop
Button Event
```

GPIO Edge以及EventLoop集成留到后续版本.

## Build集成

Make当前支持:

```bash
make gpio-probe
```

GPIO Probe会同时编译:

```text
tools/gpio_probe.c
src/gpio_line.c
```

当前:

```bash
make check
```

会验证GPIO Probe可以正常编译.

不会在自动测试中访问真实:

```text
/dev/gpiochip*
```

因此Build以及CI不依赖真实Orange Pi GPIO硬件.

## CMake集成

CMake当前新增:

```text
gpio_probe
```

Target.

GPIO Probe保持为独立Executable.

当前没有将:

```text
src/gpio_line.c
```

加入:

```text
minishell_core
```

当前Runtime Core不会直接依赖GPIO.

GPIO Runtime Device集成留到后续Device Layer阶段.

当前CMake验证:

```bash
cmake -S . -B build/cmake-v18     -DCMAKE_BUILD_TYPE=Debug     -DMINISHELL_WARNINGS_AS_ERRORS=ON
```

Build:

```bash
cmake --build build/cmake-v18 -j$(nproc)
```

当前:

```text
gpio_probe
minishell
minishell_tests
minishell_integration_tests
```

均可以正常构建.

CTest:

```bash
cd build/cmake-v18
ctest --output-on-failure
```

当前结果:

```text
minishell_unit_tests         PASS
minishell_integration_tests  PASS

100% tests passed
0 tests failed
```

## 当前GPIO架构

当前GPIO部分结构:

```text
gpio_probe
 |
gpio_line
 |
Linux GPIO Character Device
 |
Rockchip GPIO Driver
 |
RK3588 GPIO
```

当前gpio_line属于底层GPIO访问基础.

当前没有加入:

```text
DeviceManager
Device
DeviceOps
ButtonDevice
GPIO Builtin
GPIO Command
```

相关设备抽象留到后续Device Layer统一实现.

## 当前最终状态

```text
GPIO Controller确认              PASS
40Pin映射确认                    PASS
GPIO1_D6映射确认                 PASS
Button物理连接                   PASS
Internal Pull-up                PASS
wiringOP Input验证              PASS
GPIO Character Device验证       PASS
gpio_line访问层                 PASS
gpio_probe单次读取              PASS
gpio_probe持续读取              PASS
Make构建                        PASS
Strict Build                    PASS
CMake构建                       PASS
CTest                           PASS
真实Button验证                  PASS
```

