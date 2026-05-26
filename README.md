# elib-simbus

嵌入式模拟总线库 — 纯 C99，零动态内存分配，无外部依赖。通过用户注册的回调接管底层 GPIO，在软件中精确模拟总线时序。

## 模块列表

| 模块 | 头文件 | 说明 | 用法文档 |
|------|--------|------|----------|
| err | `elib_simbus_err.h` | 统一错误码 | — |
| i2c | `elib_simbus_i2c.h` | I2C 主机位敲模拟 | [docs/usage_i2c.md](docs/usage_i2c.md) |
| spi | `elib_simbus_spi.h` | SPI 主机位敲模拟，支持 Mode 0-3 | [docs/usage_spi.md](docs/usage_spi.md) |
| uart | `elib_simbus_uart.h` | UART 位敲收发，支持 5-9 位/奇偶校验/停位 | [docs/usage_uart.md](docs/usage_uart.md) |
| ow | `elib_simbus_ow.h` | 1-Wire 主机，含存在检测/位敲/字节收发 | — |
| ws2812 | `elib_simbus_ws2812.h` | WS2812/NeoPixel LED 驱动 | [docs/usage_ws2812.md](docs/usage_ws2812.md) |

用户只需 `#include "elib_simbus.h"` 即可引入全部模块，也可单独引用子模块头文件。

## 目录结构

```
elib-simbus/
├── include/
│   ├── elib_simbus.h                  # 伞形头文件
│   ├── elib_simbus_err.h              # 错误码
│   ├── elib_simbus_i2c.h              # I2C 模块
│   ├── elib_simbus_spi.h              # SPI 模块
│   ├── elib_simbus_uart.h             # UART 模块
│   ├── elib_simbus_ow.h               # 1-Wire 模块
│   └── elib_simbus_ws2812.h           # WS2812 驱动
├── src/
│   ├── elib_simbus_{i2c,spi,uart,ow,ws2812}_core.h  # 内部桥接头
│   └── elib_simbus_{i2c,spi,uart,ow,ws2812}_core.c  # 实现
├── test/
│   ├── test_elib_simbus_i2c.c         # 23 个 I2C 测试
│   ├── test_elib_simbus_spi.c         # 14 个 SPI 测试
│   ├── test_elib_simbus_uart.c        # 19 个 UART 测试
│   ├── test_elib_simbus_ow.c          # 16 个 1-Wire 测试
│   └── test_elib_simbus_ws2812.c      # 11 个 WS2812 测试
├── docs/
│   ├── usage_i2c.md                   # I2C 用法
│   ├── usage_spi.md                   # SPI 用法
│   ├── usage_uart.md                  # UART 用法
│   └── usage_ws2812.md               # WS2812 用法
├── LICENSE
└── README.md
```

## 功能列表

### i2c — I2C 主机位敲模拟

| 函数 | 说明 |
|------|------|
| `elib_simbus_i2c_init(ctx, cfg)` | 初始化 |
| `elib_simbus_i2c_deinit(ctx)` | 反初始化 |
| `elib_simbus_i2c_write(ctx, dev_addr, data, len, max_len)` | 写数据到从机 |
| `elib_simbus_i2c_read(ctx, dev_addr, data, len, max_len)` | 从从机读数据 |
| `elib_simbus_i2c_write_mem(ctx, dev_addr, mem_addr, mem_addr_len, data, len, max_len)` | 写从机寄存器/存储器 |
| `elib_simbus_i2c_read_mem(ctx, dev_addr, mem_addr, mem_addr_len, data, len, max_len)` | 读从机寄存器/存储器 |

## 构建与测试

```bash
# I2C
gcc -std=c99 -Wall -Wextra -Iinclude -o test_i2c \
  test/test_elib_simbus_i2c.c src/elib_simbus_i2c_core.c && ./test_i2c

# SPI
gcc -std=c99 -Wall -Wextra -Iinclude -o test_spi \
  test/test_elib_simbus_spi.c src/elib_simbus_spi_core.c && ./test_spi

# UART
gcc -std=c99 -Wall -Wextra -Iinclude -o test_uart \
  test/test_elib_simbus_uart.c src/elib_simbus_uart_core.c && ./test_uart

# 1-Wire
gcc -std=c99 -Wall -Wextra -Iinclude -o test_ow \
  test/test_elib_simbus_ow.c src/elib_simbus_ow_core.c && ./test_ow

# WS2812
gcc -std=c99 -Wall -Wextra -Iinclude -o test_ws2812 \
  test/test_elib_simbus_ws2812.c src/elib_simbus_ws2812_core.c && ./test_ws2812
```

### spi — SPI 主机位敲模拟

| 函数 | 说明 |
|------|------|
| `elib_simbus_spi_init(ctx, cfg)` | 初始化 |
| `elib_simbus_spi_deinit(ctx)` | 反初始化 |
| `elib_simbus_spi_transfer(ctx, tx_data, rx_data, len, max_len)` | 全双工传输 |
| `elib_simbus_spi_write(ctx, data, len, max_len)` | 只写（丢弃接收数据）|
| `elib_simbus_spi_read(ctx, data, len, max_len)` | 只读（发送 dummy byte）|
| `elib_simbus_spi_cs_low(ctx)` | 断言片选（拉低 CS）|
| `elib_simbus_spi_cs_high(ctx)` | 取消片选（拉高 CS）|

### uart — UART 位敲收发

| 函数 | 说明 |
|------|------|
| `elib_simbus_uart_init(ctx, cfg)` | 初始化 |
| `elib_simbus_uart_deinit(ctx)` | 反初始化 |
| `elib_simbus_uart_putchar(ctx, byte)` | 发送 1 字节 |
| `elib_simbus_uart_getchar(ctx)` | 接收 1 字节（返回 -1 超时）|
| `elib_simbus_uart_write(ctx, data, len, max_len)` | 发送多字节 |
| `elib_simbus_uart_read(ctx, data, len, max_len)` | 接收多字节（返回实收数）|

### ow — 1-Wire 主机

| 函数 | 说明 |
|------|------|
| `elib_simbus_ow_init(ctx, cfg)` | 初始化 |
| `elib_simbus_ow_deinit(ctx)` | 反初始化 |
| `elib_simbus_ow_reset(ctx)` | 总线复位 + 存在检测（返回 1/0/-1）|
| `elib_simbus_ow_write_bit(ctx, bit)` | 写 1 位 |
| `elib_simbus_ow_read_bit(ctx)` | 读 1 位 |
| `elib_simbus_ow_write_byte(ctx, byte)` | 写 1 字节（LSB first）|
| `elib_simbus_ow_read_byte(ctx)` | 读 1 字节 |
| `elib_simbus_ow_write(ctx, data, len, max_len)` | 写多字节 |
| `elib_simbus_ow_read(ctx, data, len, max_len)` | 读多字节 |

### ws2812 — WS2812/NeoPixel LED 驱动

| 函数 | 说明 |
|------|------|
| `elib_simbus_ws2812_init(ctx, cfg)` | 初始化 |
| `elib_simbus_ws2812_deinit(ctx)` | 反初始化 |
| `elib_simbus_ws2812_send(ctx, data, len)` | 发送原始字节数据 |
| `elib_simbus_ws2812_send_rgb(ctx, rgb, count)` | 发送 RGB 颜色（自动转 GRB）|

## 错误码

| 错误码 | 说明 |
|--------|------|
| `ELIB_SIMBUS_OK` | 成功 |
| `ELIB_SIMBUS_ERR_INVALID_PARAM` | 无效参数 |
| `ELIB_SIMBUS_ERR_NOT_INITIALIZED` | 未初始化 |
| `ELIB_SIMBUS_ERR_EXCEED_MAX` | 超过 max_len |
| `ELIB_SIMBUS_ERR_NACK` | 从机无应答 |
| `ELIB_SIMBUS_ERR_TIMEOUT` | 超时（超过 max_wait 轮） |

## 许可

MIT License
