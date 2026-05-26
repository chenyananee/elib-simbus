# WS2812 (NeoPixel) 驱动用法

头文件：`elib_simbus_ws2812.h`（通过 `elib_simbus.h` 自动引入）

纯软件位敲 WS2812/NeoPixel 可寻址 LED 驱动，ns 级时序控制。

---

## 原理

WS2812 使用单线归零码（NRZ）协议，每比特由高/低电平的时长组合区分 0 和 1：

```
0-code: ‾‾‾|___   高 ~350ns → 低 ~800ns
1-code: ‾‾‾‾‾‾|__  高 ~700ns → 低 ~600ns
Reset:  ___...___  低 >50µs
```

数据格式：每颗 LED 24bit，顺序 **G(8bit) + R(8bit) + B(8bit)**，MSB first。

与 I2C/SPI/UART 相同的回调模式，但延时回调为 ns 级：

| 回调 | 说明 |
|------|------|
| `io_write(pin, level)` | 设置引脚输出电平 |
| `io_read(pin)` | 读取引脚电平 |
| `io_setdir(pin, dir)` | 设置引脚方向 |
| `delay_ns(ns)` | **纳秒**级延时 |

> WS2812 时序在数十到数百 ns 级别，请确保 `delay_ns` 回调能提供足够的精度（如使用 DWT 时钟周期计数或硬件定时器）。

---

## 配置结构体

```c
typedef struct {
    uint8_t  dq_pin;
    uint32_t t0h;               /* 0-code 高电平时间 (ns) */
    uint32_t t0l;               /* 0-code 低电平时间 (ns) */
    uint32_t t1h;               /* 1-code 高电平时间 (ns) */
    uint32_t t1l;               /* 1-code 低电平时间 (ns) */
    uint32_t reset_ns;          /* 复位低电平时间 (ns) */

    elib_simbus_ws2812_io_write_t  io_write;
    elib_simbus_ws2812_io_read_t   io_read;
    elib_simbus_ws2812_io_setdir_t io_setdir;
    elib_simbus_ws2812_delay_ns_t  delay_ns;
} elib_simbus_ws2812_cfg_t;
```

默认值：

| 参数 | 默认值 | 说明 |
|------|--------|------|
| `t0h` | 350 | 0-code 高 (ns) |
| `t0l` | 800 | 0-code 低 (ns) |
| `t1h` | 700 | 1-code 高 (ns) |
| `t1l` | 600 | 1-code 低 (ns) |
| `reset_ns` | 55000 | 复位 (ns) = 55µs |

---

## API 说明

### 初始化 / 反初始化

```c
elib_simbus_err_t elib_simbus_ws2812_init(
    elib_simbus_ws2812_ctx_t *ctx,
    const elib_simbus_ws2812_cfg_t *cfg);

void elib_simbus_ws2812_deinit(elib_simbus_ws2812_ctx_t *ctx);
```

```c
elib_simbus_ws2812_init(&ctx, &(elib_simbus_ws2812_cfg_t){
    .dq_pin = 0,
    .io_write = gpio_write, .io_read = gpio_read,
    .io_setdir = gpio_setdir, .delay_ns = delay_ns,
});
```

### 发送原始数据

```c
elib_simbus_err_t elib_simbus_ws2812_send(
    elib_simbus_ws2812_ctx_t *ctx,
    const void *data,
    uint32_t len);
```

发送原始字节数据，MSB first。用户需自行按 GRB 顺序组织字节。

### 发送 RGB 颜色

```c
elib_simbus_err_t elib_simbus_ws2812_send_rgb(
    elib_simbus_ws2812_ctx_t *ctx,
    const uint8_t *rgb,
    uint32_t count);
```

按 RGB888 顺序传入颜色数据（`r0,g0,b0, r1,g1,b1, ...`），库自动转换为 WS2812 所需的 GRB 顺序。

---

## 示例

### 单颗 LED 红色

```c
#include "elib_simbus.h"

void set_red(void)
{
    elib_simbus_ws2812_ctx_t ctx;
    elib_simbus_ws2812_init(&ctx, &(elib_simbus_ws2812_cfg_t){
        .dq_pin = 0,
        .io_write = gpio_write, .io_read = gpio_read,
        .io_setdir = gpio_setdir, .delay_ns = delay_ns,
    });

    uint8_t rgb[] = {0xFF, 0x00, 0x00};  /* R, G, B */
    elib_simbus_ws2812_send_rgb(&ctx, rgb, 1);
}
```

### 多颗 LED 彩虹

```c
uint8_t leds[] = {
    0xFF, 0x00, 0x00,   /* LED 0: red */
    0x00, 0xFF, 0x00,   /* LED 1: green */
    0x00, 0x00, 0xFF,   /* LED 2: blue */
};
elib_simbus_ws2812_send_rgb(&ctx, leds, 3);
```

### 直接发送原始 GRB 数据

```c
/* 已按 GRB 顺序组织的字节 */
uint8_t grb_data[] = {0x00, 0xFF, 0x00};  /* G=0, R=255, B=0 */
elib_simbus_ws2812_send(&ctx, grb_data, 3);
```

---

## 时序说明

每 bit 耗时：

| 码型 | 高 (ns) | 低 (ns) | 总计 (ns) |
|------|---------|---------|----------|
| 0 | 350 | 800 | 1150 |
| 1 | 700 | 600 | 1300 |

每 LED (24bit) 耗时约 28.8µs（全部 0-code）至 31.2µs（全部 1-code）。
复位脉冲 55µs。

多 LED 级联时，数据依次发送，每颗 LED 自动将剩余数据转发至下一颗。
