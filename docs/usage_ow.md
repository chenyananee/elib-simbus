# 1-Wire 模块用法

头文件：`elib_simbus_ow.h`（通过 `elib_simbus.h` 自动引入）

纯软件位敲 1-Wire 主机，支持标准时序的复位/存在检测、位读写和字节读写。

---

## 原理

1-Wire 为单线开漏总线，通过严格时序槽（time slot）通信：

| 阶段 | 主机操作 | 时序 |
|------|----------|------|
| 复位 | 拉低 480µs → 释放 | 从机在 15-60µs 内拉低表示存在 |
| 写 1 | 拉低 6µs → 释放 | 释放后从机读到 1 |
| 写 0 | 拉低 60µs → 释放 | 长低电平表示 0 |
| 读 | 拉低 6µs → 释放 → 采样 | 15µs 窗口内采样 |

所有时序基于 `delay_num` 倍率：实际延时 = `delay_num × 倍率`（默认 `delay_num = 1µs`）。

---

## 配置结构体

```c
typedef struct {
    uint8_t  dq_pin;
    uint32_t delay_num;          /* 时基 (µs), 标准 = 1 */
    uint32_t timeout_rounds;     /* 存在脉冲检测超时轮数 */

    elib_simbus_ow_io_write_t  io_write;
    elib_simbus_ow_io_read_t   io_read;
    elib_simbus_ow_io_setdir_t io_setdir;
    elib_simbus_ow_delay_us_t  delay_us;
} elib_simbus_ow_cfg_t;
```

时序倍率（不可配置，固定值）：

| 阶段 | 倍率 (× delay_num) | 默认延时 |
|------|--------------------|----------|
| 复位低 | 480 | 480 µs |
| 复位采样等待 | 70 | 70 µs |
| 写-1 低 | 6 | 6 µs |
| 写-1 槽 | 64 | 64 µs |
| 写-0 低 | 60 | 60 µs |
| 读低 | 6 | 6 µs |
| 读采样 | 9 | 9 µs |
| 读槽 | 60 | 60 µs |

---

## API 说明

### 初始化 / 反初始化

```c
elib_simbus_err_t elib_simbus_ow_init(
    elib_simbus_ow_ctx_t *ctx,
    const elib_simbus_ow_cfg_t *cfg);

void elib_simbus_ow_deinit(elib_simbus_ow_ctx_t *ctx);
```

### 总线复位 + 存在检测

```c
int elib_simbus_ow_reset(elib_simbus_ow_ctx_t *ctx);
```

| 返回值 | 含义 |
|--------|------|
| 1 | 存在从机（检测到存在脉冲）|
| -1 | 超时（无设备或总线故障）|

时序：主机拉低 480µs → 释放 → 等待 70µs → 轮询 DQ。若从机在 `timeout_rounds` 内拉低 DQ，返回 1；否则超时返回 -1。

### 位操作

```c
void elib_simbus_ow_write_bit(elib_simbus_ow_ctx_t *ctx, uint8_t bit);
uint8_t elib_simbus_ow_read_bit(elib_simbus_ow_ctx_t *ctx);
```

### 字节操作

```c
void elib_simbus_ow_write_byte(elib_simbus_ow_ctx_t *ctx, uint8_t byte);
uint8_t elib_simbus_ow_read_byte(elib_simbus_ow_ctx_t *ctx);
```

字节以 **LSB first** 顺序发送/接收。

### 多字节收发

```c
elib_simbus_err_t elib_simbus_ow_write(
    elib_simbus_ow_ctx_t *ctx,
    const void *data,
    uint32_t len,
    uint32_t max_len);

elib_simbus_err_t elib_simbus_ow_read(
    elib_simbus_ow_ctx_t *ctx,
    void *data,
    uint32_t len,
    uint32_t max_len);
```

---

## 示例

### DS18B20 温度传感器读取

```c
#include "elib_simbus.h"

/* 1-Wire 命令 */
#define OW_CMD_SKIP_ROM    0xCC
#define OW_CMD_CONVERT_T   0x44
#define OW_CMD_READ_SCRATCH 0xBE

void read_ds18b20(elib_simbus_ow_ctx_t *ctx)
{
    /* 启动温度转换 */
    elib_simbus_ow_reset(ctx);
    elib_simbus_ow_write_byte(ctx, OW_CMD_SKIP_ROM);
    elib_simbus_ow_write_byte(ctx, OW_CMD_CONVERT_T);
    /* 等待转换完成 (DS18B20 典型 750ms) */

    /* 读 Scratchpad */
    elib_simbus_ow_reset(ctx);
    elib_simbus_ow_write_byte(ctx, OW_CMD_SKIP_ROM);
    elib_simbus_ow_write_byte(ctx, OW_CMD_READ_SCRATCH);

    uint8_t scratch[9] = {0};
    elib_simbus_ow_read(&ctx, scratch, 9, 9);

    int16_t temp = (int16_t)(scratch[0] | (scratch[1] << 8));
    float celsius = temp * 0.0625f;
}
```

### 基本读写

```c
elib_simbus_ow_ctx_t ctx;
elib_simbus_ow_init(&ctx, &(elib_simbus_ow_cfg_t){
    .dq_pin = 0, .delay_num = 1,
    .io_write = gpio_write, .io_read = gpio_read,
    .io_setdir = gpio_setdir, .delay_us = delay_us,
});

if (elib_simbus_ow_reset(&ctx) == 1) {
    /* 有设备，发送命令 0xCC (Skip ROM) */
    elib_simbus_ow_write_byte(&ctx, 0xCC);

    /* 读取 2 字节 */
    uint8_t buf[2] = {0};
    elib_simbus_ow_read(&ctx, buf, 2, 2);
}
```

---

## 时序说明

### 复位 + 存在检测

```
主机: _________                ______________________
              |______________|
               \____________/
              480µs ↓   ↑ 70µs
                         |
从机:                    |________|
                         60-240µs
```

### 写时序槽

```
写 1: ‾‾|___|‾‾‾‾‾‾‾‾‾‾‾‾‾
         6µs   58µs (释放)

写 0: ‾‾|________________|‾‾‾
         60µs           恢复
```

### 读时序槽

```
读: ‾‾|___|‾‾‾‾|________|‾‾
       6µs   采样  剩余槽
               ↑
           9µs 内读 DQ
```
