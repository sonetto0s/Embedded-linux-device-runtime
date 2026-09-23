# I2C OLED文件说明

## 本文件用以记录Orange Pi 5 Plus I2C接口Bring-up以及当前OLED访问层实现情况

## 当前I2C环境

当前Orange Pi 5 Plus通过40Pin使用:

```text
I2C4_M3
```

当前Linux设备节点:

```text
/dev/i2c-4
```

执行:

```bash
i2cdetect -l
```

当前可以看到:

```text
i2c-4   unknown   rk3x-i2c   N/A
```

当前I2C4通过Rockchip rk3x-i2c驱动提供.

## I2C引脚

当前最终使用Orange Pi 5 Plus 40Pin中的:

```text
Physical Pin 22
Physical Pin 32
```

对应:

```text
Physical Pin 22 = I2C4_M3 SDA
Physical Pin 32 = I2C4_M3 SCL
```

当前gpio readall中两组引脚均处于:

```text
ALT9
```

当前I2C连接关系:

```text
Orange Pi Physical Pin 22
 |
I2C4_M3 SDA
 |
OLED SDA
```

```text
Orange Pi Physical Pin 32
 |
I2C4_M3 SCL
 |
OLED SCL
```

## OLED连接

当前使用128x64 I2C OLED进行I2C以及显示验证.

连接:

```text
OLED VCC
 |
Orange Pi Physical Pin 1
 |
3.3V
```

```text
OLED GND
 |
Orange Pi Physical Pin 6
 |
GND
```

```text
OLED SDA
 |
Orange Pi Physical Pin 22
 |
I2C4_M3 SDA
```

```text
OLED SCL
 |
Orange Pi Physical Pin 32
 |
I2C4_M3 SCL
```

当前OLED使用3.3V供电.

当前不会将OLED I2C信号连接到5V.

## I2C4启用

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
i2c4-m3
```

启用I2C4_M3.

重启以后确认:

```bash
ls -l /dev/i2c-4
```

当前设备节点正常存在.

## I2C设备扫描

当前通过:

```bash
sudo i2cdetect -y 4
```

扫描I2C4.

当前OLED地址:

```text
0x3C
```

当前扫描结果可以检测到:

```text
3c
```

因此当前最终I2C配置:

```text
I2C Bus: /dev/i2c-4
OLED Address: 0x3C
```

## I2C2_M0测试记录

最初测试过:

```text
I2C2_M0
```

对应:

```text
Physical Pin 3 = SDA
Physical Pin 5 = SCL
```

当前当时可以看到:

```text
/dev/i2c-2
```

并且gpio readall中Pin 3以及Pin 5处于I2C复用状态.

但是实际执行:

```bash
sudo i2cdetect -y 2
```

没有检测到OLED地址.

后续切换到:

```text
I2C4_M3
```

以后立即可以检测到:

```text
0x3C
```

因此当前Phase最终固定使用:

```text
I2C4_M3
/dev/i2c-4
Physical Pin 22 SDA
Physical Pin 32 SCL
```

当前不继续在I2C2_M0上进行Bring-up.

## I2C访问层

当前新增:

```text
include/i2c_bus.h
src/i2c_bus.c
```

当前I2C访问层主要接口:

```text
i2c_bus_init
i2c_bus_open
i2c_bus_probe
i2c_bus_write
i2c_bus_read
i2c_bus_close
```

I2C Bus Context当前保存:

```text
fd
address
```

当前I2C访问流程:

```text
Application
 |
i2c_bus
 |
/dev/i2c-4
 |
i2c-dev
 |
rk3x-i2c
 |
RK3588 I2C4
 |
OLED
```

当前不会直接访问RK3588 I2C寄存器.

## I2C设备打开

当前通过:

```text
open
```

打开:

```text
/dev/i2c-4
```

然后通过:

```text
ioctl
```

设置:

```text
I2C_SLAVE
```

当前OLED地址设置为:

```text
0x3C
```

文件描述符使用:

```text
FD_CLOEXEC
```

避免设备FD在exec以后意外继承.

## I2C Probe

当前新增I2C硬件验证工具:

```text
tools/i2c_probe.c
```

Make构建:

```bash
make i2c-probe
```

生成:

```text
build/default/i2c_probe
```

执行:

```bash
sudo ./build/default/i2c_probe /dev/i2c-4 0x3c
```

当前结果:

```text
I2C device detected: bus=/dev/i2c-4 address=0x3C
```

错误地址验证:

```bash
sudo ./build/default/i2c_probe /dev/i2c-4 0x3d
```

当前返回:

```text
No such device or address
```

因此当前Probe可以区分实际存在以及不存在的I2C地址.

## OLED访问层

当前新增:

```text
include/oled.h
src/oled.c
```

当前OLED分辨率:

```text
128x64
```

Framebuffer大小:

```text
1024 bytes
```

计算关系:

```text
128 * 64 / 8 = 1024
```

当前主要接口包括:

```text
oled_init
oled_clear
oled_fill
oled_set_pixel
oled_draw_ascii16
oled_draw_zh16
oled_draw_text_utf8
oled_refresh
oled_display_on
oled_display_off
```

OLED初始化以后通过Framebuffer进行像素绘制.

Framebuffer最终通过:

```text
oled_refresh
```

统一刷新到OLED.

## OLED数据传输

当前OLED I2C Command Control Byte:

```text
0x00
```

当前OLED I2C Data Control Byte:

```text
0x40
```

因此当前发送流程:

```text
OLED Command
 |
0x00
 |
Command Data
```

显示数据流程:

```text
Framebuffer
 |
0x40
 |
Pixel Data
 |
OLED
```

当前已经完成真实OLED像素显示验证.

## OLED Demo

当前新增:

```text
tools/oled_demo.c
```

Make构建:

```bash
make oled-demo
```

生成:

```text
build/default/oled_demo
```

执行:

```bash
sudo ./build/default/oled_demo /dev/i2c-4 0x3c
```

当前OLED可以正常显示中英文状态信息.

当前Demo主要用于验证:

```text
OLED初始化
Framebuffer
ASCII字符
中文字符
UTF-8字符串
I2C数据刷新
```

当前Demo保持为独立Hardware Bring-up工具.

当前没有直接加入MiniShell Runtime Core.

## UTF-8显示

当前OLED显示层支持UTF-8字符串.

当前流程:

```text
UTF-8 String
 |
UTF-8 Decode
 |
Unicode Codepoint
 |
Font Lookup
 |
Bitmap
 |
Framebuffer
 |
OLED
```

当前中文字符通过Unicode Codepoint查找16x16字模.

ASCII字符使用8x16字模.

当前OLED一行统一使用16 Pixel高度.

因此128x64 OLED当前可以显示:

```text
4 Lines
```

## OLED字体

当前字模来源使用:

```text
GNU Unifont
```

当前ASCII字模:

```text
8x16
```

当前中文字模:

```text
16x16
```

当前不再使用矢量字体直接缩放生成OLED字模.

之前尝试过通过Pillow以及Noto字体生成小尺寸字模.

实际在1bit 128x64 OLED中会出现:

```text
笔画失真
数字变形
英文字符不清晰
中文字符粘连
```

最终改为GNU Unifont原生点阵以后显示正常.

## 字模生成

当前字模生成工具:

```text
tools/gen_oled_font.py
```

Make执行:

```bash
make oled-font
```

生成:

```text
src/oled_font.c
```

头文件:

```text
include/oled_font.h
```

当前Runtime不会依赖Python或者系统字体.

Python以及GNU Unifont只在重新生成字模源码时使用.

程序实际运行时使用已经生成的:

```text
src/oled_font.c
```

## 中文字符范围

当前不会将全部Unicode中文字库直接加入程序.

当前只生成项目实际需要的中文字符.

需要增加新中文显示内容时修改:

```text
CHINESE_CHARS
```

然后执行:

```bash
make oled-font
make oled-demo
```

重新生成OLED字模以及Demo.

当前这种方式用于控制生成代码规模并保持设备状态屏字符集合明确.

## Build集成

Make当前支持:

```bash
make i2c-probe
make oled-demo
make oled-font
```

当前生成:

```text
build/default/i2c_probe
build/default/oled_demo
```

不会再在项目根目录直接生成:

```text
i2c_probe
oled_demo
```

当前:

```bash
make clean
```

会删除:

```text
build
dist
Testing
shell
gpio_probe
i2c_probe
oled_demo
```

项目源码目录保持干净.

## CMake集成

CMake当前新增:

```text
i2c_probe
oled_demo
```

Target.

当前i2c_probe主要编译:

```text
tools/i2c_probe.c
src/i2c_bus.c
```

当前oled_demo主要编译:

```text
tools/oled_demo.c
src/i2c_bus.c
src/oled.c
src/oled_font.c
```

当前没有将:

```text
src/i2c_bus.c
src/oled.c
src/oled_font.c
```

加入:

```text
minishell_core
```

当前I2C以及OLED仍然保持为V1.8 Hardware Bring-up基础.

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

当前可以正常构建:

```text
minishell
gpio_probe
i2c_probe
oled_demo
minishell_tests
minishell_integration_tests
```

CTest当前已经注册:

```text
minishell_unit_tests
minishell_integration_tests
```

当前Orange Pi Focal环境运行CTest使用:

```bash
cd build/cmake
ctest --output-on-failure
```

不会直接在源码根目录执行ctest.

## 当前I2C OLED架构

当前I2C以及OLED部分结构:

```text
i2c_probe
 |
i2c_bus
 |
Linux i2c-dev
 |
rk3x-i2c
 |
RK3588 I2C4
```

OLED部分:

```text
oled_demo
 |
UTF-8 Decode
 |
oled_font
 |
oled
 |
i2c_bus
 |
/dev/i2c-4
 |
SSD1306 Compatible OLED
```

当前i2c_bus属于底层I2C访问基础.

当前oled属于OLED显示访问基础.

当前没有加入:

```text
DeviceManager
Device
DeviceOps
OLEDDevice
OLED Dashboard
Button Page Switch
EventLoop
epoll
```

