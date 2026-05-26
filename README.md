# elib-simbus

嵌入式模拟总线库 — 纯 C99，零动态内存分配，无外部依赖。通过用户注册的回调接管底层 GPIO，在软件中精确模拟总线时序。

## 模块列表

| 模块 | 头文件 | 说明 | 用法文档 |
|------|--------|------|----------|
| err | `elib_simbus_err.h` | 统一错误码 | — |
| util | `elib_simbus_util.h` | MIN/MAX/CLAMP/BIT/CONTAINER_OF/ARRAY_SIZE/WEAK/UNUSED | — |
| i2c | `elib_simbus_i2c.h` | I2C 主机位敲模拟 | [docs/usage_i2c.md](docs/usage_i2c.md) |
| spi | `elib_simbus_spi.h` | SPI 主机位敲模拟，支持 Mode 0-3 | [docs/usage_spi.md](docs/usage_spi.md) |

用户只需 `#include "elib_simbus.h"` 即可引入全部模块，也可单独引用子模块头文件。

## 目录结构

```
elib-simbus/
├── include/
│   ├── elib_simbus.h                  # 伞形头文件
│   ├── elib_simbus_err.h              # 错误码
│   ├── elib_simbus_util.h             # 快捷宏
│   └── elib_simbus_i2c.h              # I2C 模块
├── src/
│   ├── elib_simbus_i2c_core.h         # I2C 内部桥接头
│   ├── elib_simbus_i2c_core.c         # I2C 实现
│   └── elib_simbus_util_core.h        # util 内部桥接头
├── test/
│   └── test_elib_simbus_i2c.c         # 单元测试
├── docs/
│   └── usage_i2c.md                   # I2C 用法文档
├── LICENSE
└── README.md
```

## 功能列表

### i2c — I2C 主机位敲模拟

| 函数 | 说明 |
|------|------|
| `elib_simbus_i2c_init(ctx, scl_pin, sda_pin, delay_num, max_wait, io_write, io_read, io_setdir, delay_us)` | 初始化 |
| `elib_simbus_i2c_deinit(ctx)` | 反初始化 |
| `elib_simbus_i2c_write(ctx, dev_addr, data, len, max_len)` | 写数据到从机 |
| `elib_simbus_i2c_read(ctx, dev_addr, data, len, max_len)` | 从从机读数据 |
| `elib_simbus_i2c_write_mem(ctx, dev_addr, mem_addr, mem_addr_len, data, len, max_len)` | 写从机寄存器/存储器 |
| `elib_simbus_i2c_read_mem(ctx, dev_addr, mem_addr, mem_addr_len, data, len, max_len)` | 读从机寄存器/存储器 |

## 构建与测试

```bash
gcc -std=c99 -Wall -Wextra -Iinclude -o test_elib_simbus_i2c \
  test/test_elib_simbus_i2c.c src/elib_simbus_i2c_core.c && ./test_elib_simbus_i2c
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
