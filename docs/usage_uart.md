# UART 模块用法

头文件：`elib_simbus_uart.h`（通过 `elib_simbus.h` 自动引入）

状态机驱动的纯软件位敲（Bit-Bang）UART 收发器。TX/RX 状态机独立，用户在定时器中断中分别调用 `poll_tx` / `poll_rx` 推进，每次调用最多推进一个位。

---

## 原理

与 I2C/SPI 相同的回调模式，但无需 `delay_us`——时序由用户外部传入的 `elapsed_ns` 驱动：

| 回调 | 说明 |
|------|------|
| `io_write(pin, level)` | 设置引脚输出电平 |
| `io_read(pin)` | 读取引脚电平 |
| `rx_callback(ctx, byte)` | 每收到一个字节时回调 |

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
    uint32_t bit_time_ns;       /* 位时间 ns = 1000000000 / baud */
    uint32_t data_bits;         /* 5-9，默认 8 */
    uint32_t parity;            /* 0=none, 1=odd, 2=even */
    uint32_t stop_bits;         /* 1 或 2 */

    elib_simbus_uart_io_write_t    io_write;
    elib_simbus_uart_io_read_t     io_read;
    elib_simbus_uart_rx_callback_t rx_callback;
} elib_simbus_uart_cfg_t;
```

常见波特率 `bit_time_ns` 值：

| 波特率 | bit_time_ns |
|--------|-------------|
| 9600   | 104167 |
| 19200  | 52083 |
| 38400  | 26042 |
| 57600  | 17361 |
| 115200 | 8681 |
| 230400 | 4340 |
| 1000000 | 1000 |

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
    .bit_time_ns = 8681,        /* 115200 baud */
    .data_bits = 8, .parity = 0, .stop_bits = 1,
    .io_write = gpio_write, .io_read = gpio_read,
    .rx_callback = on_rx_byte,
});
```

### 发送

```c
/* 启动发送（单字节 len=1，多字节传实际长度） */
elib_simbus_err_t elib_simbus_uart_start_tx(
    elib_simbus_uart_ctx_t *ctx, const uint8_t *data, uint32_t len);

/* 查询是否正在发送 */
uint8_t elib_simbus_uart_tx_busy(elib_simbus_uart_ctx_t *ctx);
```

`start_tx` 将 TX 引脚拉低（起始位），进入发送状态。若 TX 正忙返回 `ELIB_SIMBUS_ERR_INVALID_PARAM`。多字节发送时字节间无缝衔接（无额外空闲间隔）。

### 状态机轮询

```c
/* 推进 TX 状态机，每次最多推进一个位 */
void elib_simbus_uart_poll_tx(
    elib_simbus_uart_ctx_t *ctx, uint32_t elapsed_ns);

/* 推进 RX 状态机，每次最多推进一个位 */
void elib_simbus_uart_poll_rx(
    elib_simbus_uart_ctx_t *ctx, uint32_t elapsed_ns);
```

在定时器中断中调用，传入自上次 poll 以来经过的纳秒数。TX 和 RX 可独立调用，每次调用最多推进一个位。

---

## 状态机

### TX 状态机

```
IDLE ──(start_tx)──> START_BIT ──> DATA_BIT[0..N] ──> [PARITY] ──> STOP_BIT ──> IDLE
         TX=低         TX=数据位      TX=校验位        TX=高
```

### RX 状态机

```
IDLE ──(下降沿)──> START_BIT ──> DATA_BIT[0..N] ──> [PARITY] ──> STOP_BIT ──> 回调 ──> IDLE
       采样RX引脚    等半位采样     采样数据位         跳过         采样停止位    rx_callback
```

RX 在 IDLE 状态每次 poll 都采样 RX 引脚，检测到下降沿（高→低）后启动接收。采样点位于每位中心（1.5, 2.5, 3.5... bit times from edge）。

---

## 使用示例

### 完整示例

```c
/* RX 回调：每收到一字节自动调用 */
void on_rx_byte(elib_simbus_uart_ctx_t *ctx, uint8_t byte)
{
    rx_buf[rx_cnt++] = byte;
}

/* 初始化 */
elib_simbus_uart_ctx_t uart_ctx;
elib_simbus_uart_init(&uart_ctx, &(elib_simbus_uart_cfg_t){
    .tx_pin = 0, .rx_pin = 1,
    .bit_time_ns = 8681,        /* 115200 baud */
    .data_bits = 8, .parity = 0, .stop_bits = 1,
    .io_write = gpio_write, .io_read = gpio_read,
    .rx_callback = on_rx_byte,
});

/* 发送单字节 */
uint8_t ch = 'A';
elib_simbus_uart_start_tx(&uart_ctx, &ch, 1);

/* 发送多字节 */
const uint8_t msg[] = {0x01, 0x02, 0x03};
elib_simbus_uart_start_tx(&uart_ctx, msg, sizeof(msg));

/* 定时器中断处理函数 */
void timer_isr(void)
{
    uint32_t elapsed = timer_get_elapsed_ns();  /* 用户实现 */
    elib_simbus_uart_poll_tx(&uart_ctx, elapsed);
    elib_simbus_uart_poll_rx(&uart_ctx, elapsed);
}

/* 主循环检查发送状态 */
while (elib_simbus_uart_tx_busy(&uart_ctx)) {
    /* 等待发送完成 */
}
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

每字节耗时 = `(1 + data_bits + parity + stop_bits) × bit_time_ns`

常见配置耗时：

| 配置 | 每字节耗时 |
|------|-----------|
| 8N1 @ 115200 | 10 × 8681ns ≈ 87µs |
| 8N1 @ 9600 | 10 × 104167ns ≈ 1.04ms |
| 8E1 @ 115200 | 11 × 8681ns ≈ 95µs |
