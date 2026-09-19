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

V1.6开始在Orange Pi 5 Plus中进行ARM64原生编译以及运行.

编译:

```
make clean
make release
```

当前生成程序:

```
shell: ELF 64-bit LSB shared object, ARM aarch64
```

运行:

```
./shell
```

当前MiniShell能够正常启动以及执行外部命令.

## Thermal接口

RK3588当前提供以下thermal zone:

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

## V1.6当前进度

```
Orange Pi 5 Plus平台确认
ARM64原生编译
MiniShell板端运行
Device Tree接口确认
Thermal sysfs接口确认
```
.

