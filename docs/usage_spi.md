# SPI 模块用法

头文件：`elib_simbus_spi.h`（通过 `elib_simbus.h` 自动引入）

纯软件位敲（Bit-Bang）SPI 主机，支持全部 4 种模式，全双工传输。

---

## 原理

与 I2C 模块相同的回调模式，用户提供四个回调接口操作 GPIO：

| 回调 | 说明 |
|------|------|
| `io_write(pin, level)` | 设置引脚输出电平 |
| `io_read(pin)` | 读取引脚电平 |
| `io_setdir(pin, dir)` | 设置引脚方向（0=输入，1=输出） |
| `delay_us(us)` | 微秒级延时 |

引脚分配：

| 引脚 | 说明 |
|------|------|
| SCLK | 串行时钟（主机输出） |
| MOSI | 主机输出/从机输入 |
| MISO | 主机输入/从机输出 |
| CS | 片选（可选，由用户手动控制） |

---

## 配置结构体

```c
typedef struct {
    uint8_t  sclk_pin;
    uint8_t  mosi_pin;
    uint8_t  miso_pin;
    uint8_t  cs_pin;
    uint32_t delay_num;
    uint32_t mode;              /* 0-3 */
    uint32_t bit_order;         /* 0=MSB first, 1=LSB first */
    uint8_t  dummy_byte;        /* 读操作时发送的哑数据 */

    elib_simbus_spi_io_write_t  io_write;
    elib_simbus_spi_io_read_t   io_read;
    elib_simbus_spi_io_setdir_t io_setdir;
    elib_simbus_spi_delay_us_t  delay_us;
} elib_simbus_spi_cfg_t;
```

---

## 模式说明

| 模式 | CPOL | CPHA | SCLK 空闲电平 | 捕获边沿 | 说明 |
|------|------|------|---------------|----------|------|
| 0 | 0 | 0 | 低 | 上升沿 | 最常用 |
| 1 | 0 | 1 | 低 | 下降沿 |
| 2 | 1 | 0 | 高 | 下降沿 |
| 3 | 1 | 1 | 高 | 上升沿 |

---

## API 说明

### 初始化 / 反初始化

```c
elib_simbus_err_t elib_simbus_spi_init(
    elib_simbus_spi_ctx_t *ctx,
    const elib_simbus_spi_cfg_t *cfg);

void elib_simbus_spi_deinit(elib_simbus_spi_ctx_t *ctx);
```

```c
elib_simbus_spi_init(&ctx, &(elib_simbus_spi_cfg_t){
    .sclk_pin = 0, .mosi_pin = 1,
    .miso_pin = 2, .cs_pin = 3,
    .delay_num = 1, .mode = 0,
    .io_write = gpio_write, .io_read = gpio_read,
    .io_setdir = gpio_setdir, .delay_us = delay_us,
});
```

### 全双工传输

```c
elib_simbus_err_t elib_simbus_spi_transfer(
    elib_simbus_spi_ctx_t *ctx,
    const void *tx_data,    /* 待发送数据，NULL 则发 dummy_byte */
    void *rx_data,           /* 接收缓冲区，NULL 则丢弃 */
    uint32_t len,
    uint32_t max_len);
```

`tx_data` 和 `rx_data` 均可为 NULL。若 `tx_data` 为 NULL，发送 `dummy_byte`；若 `rx_data` 为 NULL，接收数据丢弃。

### 只写 / 只读

```c
elib_simbus_err_t elib_simbus_spi_write(
    elib_simbus_spi_ctx_t *ctx,
    const void *data,
    uint32_t len,
    uint32_t max_len);

elib_simbus_err_t elib_simbus_spi_read(
    elib_simbus_spi_ctx_t *ctx,
    void *data,
    uint32_t len,
    uint32_t max_len);
```

- `write`：发送数据，丢弃接收
- `read`：发送 `dummy_byte` 并接收从机数据

### CS 控制

```c
void elib_simbus_spi_cs_low(elib_simbus_spi_ctx_t *ctx);
void elib_simbus_spi_cs_high(elib_simbus_spi_ctx_t *ctx);
```

CS 由用户手动管理，传输前后分别调用：

```c
elib_simbus_spi_cs_low(&ctx);
elib_simbus_spi_transfer(&ctx, tx, rx, 4, 4);
elib_simbus_spi_cs_high(&ctx);
```

---

## 示例

### 全双工传输（Mode 0）

```c
#include "elib_simbus.h"

void spi_example(void)
{
    elib_simbus_spi_ctx_t ctx;

    elib_simbus_spi_init(&ctx, &(elib_simbus_spi_cfg_t){
        .sclk_pin = 0, .mosi_pin = 1,
        .miso_pin = 2, .cs_pin = 3,
        .delay_num = 1, .mode = 0,
        .io_write = gpio_write, .io_read = gpio_read,
        .io_setdir = gpio_setdir, .delay_us = delay_us,
    });

    /* 发送 3 字节，同时接收 3 字节 */
    uint8_t tx[] = {0x12, 0x34, 0x56};
    uint8_t rx[3] = {0};
    elib_simbus_spi_cs_low(&ctx);
    elib_simbus_spi_transfer(&ctx, tx, rx, 3, 3);
    elib_simbus_spi_cs_high(&ctx);
}
```

### 只写（发送命令）

```c
uint8_t cmd[] = {0x90, 0x00, 0x00};
elib_simbus_spi_cs_low(&ctx);
elib_simbus_spi_write(&ctx, cmd, 3, 3);
elib_simbus_spi_cs_high(&ctx);
```

### 只读（读取数据）

```c
uint8_t rx[8] = {0};
elib_simbus_spi_cs_low(&ctx);
elib_simbus_spi_read(&ctx, rx, 8, 8);
elib_simbus_spi_cs_high(&ctx);
/* rx[0..7] 包含从机返回的 8 字节 */
```

---

## SPI 时序说明

`delay_num` 定义 SCLK 半周期。每比特时序如下：

**Mode 0（CPOL=0, CPHA=0）：**

```
SCLK:  __|‾‾|__|‾‾|__|‾‾|__
MOSI:  ----<bit7>---<bit6>---
        ^ set    ^ capture
```

每个字节约 `8 × 2 × delay_num` 延时。

**Mode 1（CPOL=0, CPHA=1）：**

```
SCLK:  __|‾‾|__|‾‾|__|‾‾|__
MOSI:  ----<bit7>---<bit6>---
        ^ change ^ capture（下降沿）
```
