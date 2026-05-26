# UART 模块用法

头文件：`elib_simbus_uart.h`（通过 `elib_simbus.h` 自动引入）

纯软件位敲（Bit-Bang）UART 收发器，支持标准异步串行协议。

---

## 原理

与 I2C/SPI 相同的回调模式：

| 回调 | 说明 |
|------|------|
| `io_write(pin, level)` | 设置引脚输出电平 |
| `io_read(pin)` | 读取引脚电平 |
| `io_setdir(pin, dir)` | 设置引脚方向 |
| `delay_us(us)` | 微秒级延时 |

引脚分配：

| 引脚 | 说明 |
|------|------|
| TX | 发送数据（主机输出） |
| RX | 接收数据（主机输入） |

---

## 配置结构体

```c
typedef struct {
    uint8_t  tx_pin;
    uint8_t  rx_pin;
    uint32_t bit_time_us;       /* 位时间 µs = 1000000 / baud */
    uint32_t data_bits;         /* 5-9，默认 8 */
    uint32_t parity;            /* 0=none, 1=odd, 2=even */
    uint32_t stop_bits;         /* 1 或 2 */
    uint32_t timeout_rounds;    /* RX 起始位检测超时轮数 */

    elib_simbus_uart_io_write_t  io_write;
    elib_simbus_uart_io_read_t   io_read;
    elib_simbus_uart_io_setdir_t io_setdir;
    elib_simbus_uart_delay_us_t  delay_us;
} elib_simbus_uart_cfg_t;
```

---

## API 说明

### 初始化 / 反初始化

```c
elib_simbus_err_t elib_simbus_uart_init(
    elib_simbus_uart_ctx_t *ctx,
    const elib_simbus_uart_cfg_t *cfg);

void elib_simbus_uart_deinit(elib_simbus_uart_ctx_t *ctx);
```

```c
elib_simbus_uart_init(&ctx, &(elib_simbus_uart_cfg_t){
    .tx_pin = 0, .rx_pin = 1,
    .bit_time_us = 1000000 / 115200,   /* 115200 baud → ~8.7µs */
    .data_bits = 8, .parity = 0, .stop_bits = 1,
    .io_write = gpio_write, .io_read = gpio_read,
    .io_setdir = gpio_setdir, .delay_us = delay_us,
});
```

### 单字节收发

```c
/* 发送 */
elib_simbus_err_t elib_simbus_uart_putchar(
    elib_simbus_uart_ctx_t *ctx, uint8_t byte);

/* 接收：返回 0-255 成功，-1 超时 */
int32_t elib_simbus_uart_getchar(
    elib_simbus_uart_ctx_t *ctx);
```

`getchar` 阻塞等待 RX 起始位（下降沿），检测到后采样数据位。若在 `timeout_rounds` 内未检测到起始位，返回 `-1` 并置 `bit_flags.timeout`。

### 多字节收发

```c
elib_simbus_err_t elib_simbus_uart_write(
    elib_simbus_uart_ctx_t *ctx,
    const void *data,
    uint32_t len,
    uint32_t max_len);

/* 返回实际接收字节数，超时返回 0 */
int32_t elib_simbus_uart_read(
    elib_simbus_uart_ctx_t *ctx,
    void *data,
    uint32_t len,
    uint32_t max_len);
```

`read` 内部循环调用 `getchar`，超时则提前返回已收字节数。

---

## 示例

### 发送

```c
elib_simbus_uart_putchar(&ctx, 'A');
```

### 接收

```c
int32_t c = elib_simbus_uart_getchar(&ctx);
if (c >= 0) {
    /* 收到字节 */
} else {
    /* 超时 */
}
```

### 发送字符串

```c
const char *msg = "Hello\n";
elib_simbus_uart_write(&ctx, msg, strlen(msg), strlen(msg));
```

---

## 帧格式

UART 帧（8N1 示例）：

```
TX/RX: ‾‾\__/‾\__/‾‾\__/‾‾‾‾\__/‾‾‾‾‾
        idle ST 0   1   2   3   4   5   6   7 SP idle
             ↑
         起始位(低)              停止位(高)
```

时序流程（发送 `0x4D` = `01001101`，LSB first）：

| 阶段 | TX | 说明 |
|------|----|------|
| 空闲 | 高 | |
| 起始位 | 低 | 1 bit_time |
| bit0 | 1 | LSB |
| bit1 | 0 | |
| bit2 | 1 | |
| bit3 | 0 | |
| bit4 | 0 | |
| bit5 | 1 | |
| bit6 | 0 | |
| bit7 | 1 | MSB |
| 停止位 | 高 | 1 bit_time |

### 发送时序

每字节耗时 = `(1 + data_bits + parity + stop_bits) × bit_time_us`

常见配置耗时：

| 配置 | 每字节耗时 |
|------|-----------|
| 8N1 @ 115200 | 10 × 8.7µs ≈ 87µs |
| 8N1 @ 9600 | 10 × 104µs ≈ 1.04ms |
| 8E1 @ 115200 | 11 × 8.7µs ≈ 96µs |

### 超时机制

`getchar` 在 RX 起始位检测阶段轮询 RX 引脚。每轮调用一次 `delay_us(bit_time_us)`，超过 `timeout_rounds` 轮仍未检测到下降沿则超时。默认 `timeout_rounds = 10000`，对于 115200 baud 约等待 87ms。
