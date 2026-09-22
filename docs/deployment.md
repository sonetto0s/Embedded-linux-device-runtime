# deployment文件说明

## 本文件用以记录MiniShell的部署方式,安装目录以及嵌入式Linux rootfs部署流程

## 当前部署结构

当前正式部署流程默认安装位置为:

```
/opt/minishell/
├── bin/
│   └── minishell
└── config/
    └── config.conf
```

其中:

```
bin/minishell
```

为MiniShell主程序.

```
config/config.conf
```

为运行时配置文件.

MiniShell在部署环境下会根据当前可执行文件位置查找配置文件.

例如程序位于:

```
/opt/minishell/bin/minishell
```

则会自动查找:

```
/opt/minishell/config/config.conf
```

因此部署完成后MiniShell不需要依赖源码目录运行.

## Makefile安装

默认安装路径:

```
/opt/minishell
```

正常安装:

```
make
sudo make install
```

安装完成后生成:

```
/opt/minishell/bin/minishell
/opt/minishell/config/config.conf
```

程序权限:

```
0755
```

配置文件权限:

```
0644
```

如果需要改变安装目录,可以修改PREFIX:

```
sudo make install PREFIX=/opt/myminishell
```

此时安装结构变为:

```
/opt/myminishell/
├── bin/
│   └── minishell
└── config/
    └── config.conf
```

## DESTDIR暂存部署

为了方便后续嵌入式Linux rootfs制作,Makefile支持DESTDIR.

例如:

```
make install DESTDIR=/tmp/minishell-root
```

此时不会直接修改当前Linux系统的`/opt`目录.

实际生成:

```
/tmp/minishell-root/opt/minishell/bin/minishell
/tmp/minishell-root/opt/minishell/config/config.conf
```

这里:

```
DESTDIR=/tmp/minishell-root
PREFIX=/opt/minishell
```

DESTDIR只代表当前开发机上的临时根文件系统.

最终部署到目标Linux系统以后,真实路径仍然是:

```
/opt/minishell/bin/minishell
/opt/minishell/config/config.conf
```

整体关系:

```
MiniShell源码
 |
编译
 |
MiniShell可执行文件
 |
make install
 |
DESTDIR临时rootfs
 |
目标Linux rootfs
 |
/opt/minishell
```

这种方式后续可以继续用于Buildroot等嵌入式Linux根文件系统部署.

## 卸载

正常安装后可以使用:

```
sudo make uninstall
```

如果之前使用DESTDIR进行暂存安装:

```
make uninstall DESTDIR=/tmp/minishell-root
```

uninstall只删除MiniShell自身安装的:

```
bin/minishell
config/config.conf
```

以及删除完成后已经为空的MiniShell目录.

不会直接使用`rm -rf`删除整个系统安装路径.

## Package部署

当前Makefile仍然保留package功能.

生成本机部署包:

```
make package
```

生成目录:

```
dist/native/
├── bin/
│   └── minishell
└── config/
    └── config.conf
```

ARM64交叉编译部署包:

```
make arm64-package
```

生成:

```
dist/arm64/
├── bin/
│   └── minishell
└── config/
    └── config.conf
```

package主要用于生成可以直接复制到目标设备的程序目录.

install主要用于按照Linux目标文件系统结构进行正式安装.

两种方式用途不同,但是当前统一使用相同的:

```
bin/
config/
```

目录结构.

## CMake安装

当前保持Makefile与CMake部署方式一致.

首先配置以及编译:

```
cmake -S . -B build/cmake \
    -DCMAKE_BUILD_TYPE=Release \
    -DMINISHELL_WARNINGS_AS_ERRORS=ON

cmake --build build/cmake --parallel
```

直接安装到目标系统:

```
sudo cmake --install build/cmake --prefix /opt/minishell
```

同样生成:

```
/opt/minishell/bin/minishell
/opt/minishell/config/config.conf
```

如果需要安装进临时rootfs:

```
DESTDIR=/tmp/minishell-root \
cmake --install build/cmake --prefix /opt/minishell
```

实际生成:

```
/tmp/minishell-root/opt/minishell/bin/minishell
/tmp/minishell-root/opt/minishell/config/config.conf
```

因此当前Makefile与CMake的最终部署结构保持一致.

## 配置文件查找

当前MiniShell配置文件查找顺序为:

```
MINISHELL_CONFIG
 |
程序部署目录下的config/config.conf
 |
开发环境config/config.conf
 |
当前工作目录config/config.conf
 |
程序内部默认配置
```

显式设置`MINISHELL_CONFIG`时优先使用该路径.

正常部署以后主要使用:

```
<MiniShell程序目录>/../config/config.conf
```

因此即使当前工作目录发生变化,MiniShell仍然可以找到自身部署配置.

例如:

```
cd /tmp
/opt/minishell/bin/minishell
```

仍然可以正常读取:

```
/opt/minishell/config/config.conf
```

## 硬件权限

当前已经包含Device Tree、Hardware Info、Runtime Monitor以及sysfs硬件访问能力.

只读接口通常可以由普通用户直接读取.

例如:

```
sysinfo
dtinfo
hwinfo
led list
led info blue_led
```

涉及sysfs写操作时仍然遵守Linux自身权限机制.

例如:

```
led on blue_led
led off blue_led
```

普通用户没有对应sysfs写权限时会返回权限错误.

MiniShell不会自动修改sysfs权限,也不会主动进行权限提升.

需要管理员权限时由用户自行使用:

```
sudo /opt/minishell/bin/minishell
```

## 部署验证

可以使用临时rootfs完成部署测试:

```
rm -rf /tmp/minishell-root

make install DESTDIR=/tmp/minishell-root
```

查看部署结果:

```
find /tmp/minishell-root/opt/minishell -maxdepth 2 -type f -print
```

正常应该得到:

```
/tmp/minishell-root/opt/minishell/bin/minishell
/tmp/minishell-root/opt/minishell/config/config.conf
```

可以离开源码目录进行运行验证:

```
cd /tmp

/tmp/minishell-root/opt/minishell/bin/minishell
```


## 当前部署能力

```
Make原生编译
ARM64原生编译
ARM64交叉编译
Make部署包
ARM64部署包
Make install
Make uninstall
PREFIX安装路径
DESTDIR暂存rootfs
CMake install
运行目录无关配置查找
Orange Pi 5 Plus板端部署
```
