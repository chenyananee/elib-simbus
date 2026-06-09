# SB 模块用法（自定义单总线协议）

头文件：`elib_simbus_sb.h`（通过 `elib_simbus.h` 自动引入）

状态机驱动的半双工单总线协议。TX/RX 互斥，用户在定时器中断中调用 `poll(elapsed_ns)` 推进状态机。

---

## 帧格式

```
IDLE(high) → START(low) → D0 D1 D2 D3 D4 D5 D6 D7 → P0 P1 P2 P3 → STOP(high)
              1 bit        8 data bits (LSB first)    4 parity      1 bit
```

总帧长：14 位。

### 4 位校验

| 校验位 | 覆盖范围 | 说明 |
|--------|---------|------|
| P0 | D0, D1, D2, D3 | 低 4 位偶校验 |
| P1 | D4, D5, D6, D7 | 高 4 位偶校验 |
| P2 | D0, D4, D1, D5 | 列偶校验 |
| P3 | D2, D6, D3, D7 | 列偶校验 |

---

## 原理

| 回调 | 说明 |
|------|------|
| `io_write(pin, level)` | 设置引脚输出电平 |
| `io_read(pin)` | 读取引脚电平 |
| `io_setdir(pin, dir)` | 设置引脚方向（0=输入，1=输出）|
| `rx_callback(ctx, byte)` | 每收到一个字节时回调 |

方向控制：

| 状态 | 引脚方向 | 说明 |
|------|---------|------|
| 空闲 / 空闲检测 / 接收 | INPUT | 释放总线，读取电平 |
| 发送（起始位～停止位） | OUTPUT | 驱动总线 |
| 发送完成 | INPUT | 释放总线 |

---

## 配置结构体

```c
typedef struct {
    uint8_t  pin;               /* 半双工总线引脚 */
    uint32_t bit_time_ns;       /* 位时间 (ns) */

    elib_simbus_sb_io_write_t  io_write;
    elib_simbus_sb_io_read_t   io_read;
    elib_simbus_sb_io_setdir_t io_setdir;
    elib_simbus_sb_rx_callback_t rx_callback;
} elib_simbus_sb_cfg_t;
```

---

## API 说明

### 初始化 / 反初始化

```c
elib_simbus_err_t elib_simbus_sb_init(
    elib_simbus_sb_ctx_t *ctx,
    const elib_simbus_sb_cfg_t *cfg);

void elib_simbus_sb_deinit(elib_simbus_sb_ctx_t *ctx);
```

`init` 将引脚设为输入（释放总线）。`deinit` 确保引脚回到输入状态。

### 发送

```c
/* 启动发送（单字节 len=1，多字节传实际长度） */
elib_simbus_err_t elib_simbus_sb_start_tx(
    elib_simbus_sb_ctx_t *ctx, const uint8_t *data, uint32_t len);

/* 查询是否正在发送 */
uint8_t elib_simbus_sb_tx_busy(elib_simbus_sb_ctx_t *ctx);
```

`start_tx` 进入空闲检测状态，总线必须保持高电平 15 个位时间后才开始发送。若总线忙或 TX/RX 进行中，返回 `ELIB_SIMBUS_ERR_INVALID_PARAM`。

### 状态机轮询

```c
void elib_simbus_sb_poll(
    elib_simbus_sb_ctx_t *ctx, uint32_t elapsed_ns);
```

在定时器中断中调用，传入自上次 poll 以来经过的纳秒数。TX 和 RX 共用一个状态机，互斥运行。

---

## 状态机

```
                     start_tx()
        IDLE ◄──────────────────── TX_IDLE_CHECK → TX_START_BIT → TX_DATA_BIT×8
         │                                                      → TX_PARITY_BIT×4
         │  falling edge                                        → TX_STOP_BIT ──┘
         │
         └──► RX_START_BIT → RX_DATA_BIT×8 → RX_PARITY_BIT×4 → RX_STOP_BIT → 回调 ──┘
```

### TX 流程

1. `start_tx` → `TX_IDLE_CHECK`：读总线，高电平持续 15 位时间则通过
2. 若检测到低电平（总线忙），倒计时重置，继续等待
3. 空闲确认 → 拉低总线（起始位）→ 逐位发送数据/校验/停止
4. 停止位后释放总线（切输入），回到 `IDLE`
5. 多字节发送时，停止位后自动回到 `TX_IDLE_CHECK` 重新检测空闲

### RX 流程

1. `IDLE` 状态每次 poll 采样引脚，检测下降沿（高→低）
2. 检测到后预载半位时间到累加器，实现位中心采样
3. 逐位接收数据/校验，停止位完成后调用 `rx_callback`
4. 回到 `IDLE`，更新 `prev_level` 为当前电平

---

## 使用示例

```c
/* RX 回调 */
void on_rx_byte(elib_simbus_sb_ctx_t *ctx, uint8_t byte)
{
    rx_buf[rx_cnt++] = byte;
}

/* 初始化 */
elib_simbus_sb_ctx_t sb_ctx;
elib_simbus_sb_init(&sb_ctx, &(elib_simbus_sb_cfg_t){
    .pin = GPIO_PIN_5,
    .bit_time_ns = 10000,       /* 100 kbps */
    .io_write = gpio_write,
    .io_read  = gpio_read,
    .io_setdir = gpio_setdir,
    .rx_callback = on_rx_byte,
});

/* 发送单字节 */
uint8_t ch = 0xA5;
elib_simbus_sb_start_tx(&sb_ctx, &ch, 1);

/* 发送多字节 */
const uint8_t msg[] = {0x01, 0x02, 0x03};
elib_simbus_sb_start_tx(&sb_ctx, msg, sizeof(msg));

/* 定时器中断 */
void timer_isr(void)
{
    uint32_t elapsed = timer_get_elapsed_ns();
    elib_simbus_sb_poll(&sb_ctx, elapsed);
}

/* 主循环等待发送完成 */
while (elib_simbus_sb_tx_busy(&sb_ctx)) { }
```

---

## 时序

每字节耗时 = `14 × bit_time_ns`（1 起始 + 8 数据 + 4 校验 + 1 停止）

多字节发送时，字节间额外增加 15 位空闲检测时间。

| 速率 | bit_time_ns | 单字节耗时 | 字节间空闲 |
|------|-------------|-----------|-----------|
| 100 kbps | 10000 | 140µs | 150µs |
| 50 kbps | 20000 | 280µs | 300µs |
| 10 kbps | 100000 | 1.4ms | 1.5ms |
