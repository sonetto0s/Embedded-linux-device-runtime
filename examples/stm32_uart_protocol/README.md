# STM32 UART Protocol文件说明

## 本目录提供V1.8 STM32 UART协议处理基础

当前文件:

```text
stm32_uart_protocol.h
stm32_uart_protocol.c
```

当前实现只依赖标准C库.

当前不会假定STM32型号、UART编号、LED引脚以及HAL头文件.

当前不会提供未经开发板验证的烧录工程.

## 当前协议

当前UART使用115200 8N1以及3.3V逻辑电平.

请求以及回复通过换行分隔.

处理器接收LF以及CRLF,发送LF.

```text
Request                      Reply
PING                         PONG
STATUS                       STATUS LED=0 UPTIME_MS=1234
LED ON                       OK LED=1
LED OFF                      OK LED=0
HEARTBEAT 42                 OK HEARTBEAT=42
```

STATUS的LED以及UPTIME_MS来自调用者提供的真实状态.

HEARTBEAT为单次请求以及回复,不会启动后台定时任务.

HEARTBEAT数值以及UPTIME_MS使用uint32_t.

有效数值范围:

```text
0 - 4294967295
```

数值只接受十进制数字,不会接受负号以及附加空格.

每行最多127个字符,不包含CR、LF以及字符串结尾.

错误回复:

```text
ERR COMMAND
ERR ARGUMENT
ERR DEVICE
ERR LINE_TOO_LONG
```

超长或者非法输入会丢弃到下一个换行,然后重新接收下一条请求.

Linux端会将错误回复以及不匹配回复作为EPROTO报告.

## HAL接入

在已有STM32工程中加入本目录的.c以及.h文件.

提供Stm32UartCallbacks中的四个回调:

```text
set_led       设置实际LED,成功返回0
get_led       读取LED状态,返回0或者1,失败返回负值
uptime_ms     返回当前设备启动时间,例如HAL_GetTick()
send          发送全部回复字节,成功返回0
```

若LED使用低电平点亮,在set_led/get_led中转换实际电平以及逻辑状态.

send可以使用已有UART的HAL_UART_Transmit,并将HAL_OK转换为0.

send必须发送指定size的全部字节,失败返回负值.

初始化:

```c
Stm32UartProtocol protocol;
Stm32UartCallbacks callbacks =
{
    board_set_led,
    board_get_led,
    board_uptime_ms,
    board_uart_send,
    &board_context
};

if (stm32_uart_protocol_init(&protocol, &callbacks) < 0)
{
    /* Handle invalid callbacks. */
}
```

在主循环中将UART收到的每个字节交给:

```c
stm32_uart_protocol_feed(&protocol, received_byte);
```

使用中断或者DMA接收时,接收回调只负责保存字节或者数据块.

主循环再取出字节并调用协议处理器.

不要在UART接收中断中调用可能阻塞的send回调.

本处理器不负责中断队列以及DMA缓冲区管理.

调用者需要检查feed返回值,处理UART发送失败以及接收缓冲区溢出.

## STM32实机验收

Orange Pi使用已启用的UART3_M1以及/dev/ttyS3.

STM32端UART以及引脚由实际工程配置确认.

```text
Orange Pi TX -> STM32 RX
Orange Pi RX <- STM32 TX
Orange Pi GND -> STM32 GND
```

双方使用3.3V逻辑电平,STM32通过自己的供电路径供电.

验收需要记录:

```text
STM32型号
UART编号以及引脚
固件版本
Orange Pi配置
PING/PONG
STATUS真实LED状态以及启动时间
LED ON/OFF物理状态
HEARTBEAT数值匹配
断线超时以及重新连接
```

当前代码以及主机模拟测试不会替代STM32真实硬件验收.

## 当前边界

当前协议是一问一答,每次只允许一个未完成请求.

Linux端请求前会清理旧输入,并对写入以及接收使用同一个总期限.

当前不会提供并发事务、完整帧CRC、自动重连以及后台心跳.

PING、STATUS以及LED回复没有事务编号.

某次请求超时后,如果旧回复在下一次清理之后才到达,仍可能干扰下一次事务.

实机测试需要确认STM32处理时间,发生超时后先恢复通信状态再发送新请求.

EventLoop以及epoll整合留到后续版本.
