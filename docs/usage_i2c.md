# I2C 模块用法

头文件：`elib_simbus_i2c.h`（通过 `elib_simbus.h` 自动引入）

纯软件位敲（Bit-Bang）I2C 主机，零动态分配，通过用户注册的回调接口控制底层 GPIO。

---

## 原理

库不直接操作硬件，用户提供四个回调函数，库在恰当的时序点调用它们来生成 I2C 协议波形：

| 回调 | 说明 |
|------|------|
| `io_write(pin, level)` | 设置引脚输出电平（0/1） |
| `io_read(pin)` | 读取引脚电平（返回 0/1） |
| `io_setdir(pin, dir)` | 设置引脚方向（0=输入，1=输出） |
| `delay_us(us)` | 微秒级延时 |

> 对于纯软件仿真（无硬件引脚），回调可直接操作模拟状态变量。参考 `test/test_elib_simbus_i2c.c` 中的 mock 实现。

---

## 上下文结构

```c
typedef struct {
    uint8_t  scl_pin;                    /* SCL 引脚标识 */
    uint8_t  sda_pin;                    /* SDA 引脚标识 */
    uint32_t delay_num;                  /* delay_us 的入参，控制时序间隔 */
    uint32_t max_wait;                   /* 最大等待轮次（0=使用默认 100）*/
    uint32_t wait_rounds;                /* 当前已等待轮次（内部使用）*/
    elib_simbus_i2c_io_write_t  io_write;
    elib_simbus_i2c_io_read_t   io_read;
    elib_simbus_i2c_io_setdir_t io_setdir;
    elib_simbus_i2c_delay_us_t  delay_us;
    struct {
        uint8_t initialized : 1;         /* 初始化标志 */
        uint8_t timeout     : 1;         /* 超时标志 */
        uint8_t reserved    : 6;
    } bit_flags;
} elib_simbus_i2c_ctx_t;
```

---

## API 说明

### 初始化 / 反初始化

```c
elib_simbus_err_t elib_simbus_i2c_init(
    elib_simbus_i2c_ctx_t *ctx,
    uint8_t scl_pin,
    uint8_t sda_pin,
    uint32_t delay_num,
    uint32_t max_wait,
    elib_simbus_i2c_io_write_t io_write,
    elib_simbus_i2c_io_read_t io_read,
    elib_simbus_i2c_io_setdir_t io_setdir,
    elib_simbus_i2c_delay_us_t delay_us);

void elib_simbus_i2c_deinit(elib_simbus_i2c_ctx_t *ctx);
```

- `delay_num`：传入 `delay_us` 的参数。例如 100kHz I2C 需要 5µs 半周期，则 `delay_num = 5`，`delay_us(5)` 应延时 5µs。
- `max_wait`：最大等待轮次，超过后返回 `ELIB_SIMBUS_ERR_TIMEOUT`。传 `0` 使用默认值 100。

### 写数据

```c
elib_simbus_err_t elib_simbus_i2c_write(
    elib_simbus_i2c_ctx_t *ctx,
    uint8_t dev_addr,       /* 7 位从机地址 */
    const void *data,       /* 待写数据 */
    uint32_t len,           /* 请求写入字节数 */
    uint32_t max_len);      /* 最大允许字节数 */
```

### 读数据

```c
elib_simbus_err_t elib_simbus_i2c_read(
    elib_simbus_i2c_ctx_t *ctx,
    uint8_t dev_addr,       /* 7 位从机地址 */
    void *data,             /* 接收缓冲区 */
    uint32_t len,           /* 请求读取字节数 */
    uint32_t max_len);      /* 最大允许字节数 */
```

### 写存储器 / 寄存器

```c
elib_simbus_err_t elib_simbus_i2c_write_mem(
    elib_simbus_i2c_ctx_t *ctx,
    uint8_t dev_addr,       /* 7 位从机地址 */
    uint32_t mem_addr,      /* 存储器地址 */
    uint32_t mem_addr_len,  /* 地址字节数（1-4）*/
    const void *data,       /* 待写数据 */
    uint32_t len,           /* 请求写入字节数 */
    uint32_t max_len);      /* 最大允许字节数 */
```

内部时序：`START + addr(W) + mem_addr(MSB first) + data + STOP`

### 读存储器 / 寄存器

```c
elib_simbus_err_t elib_simbus_i2c_read_mem(
    elib_simbus_i2c_ctx_t *ctx,
    uint8_t dev_addr,       /* 7 位从机地址 */
    uint32_t mem_addr,      /* 存储器地址 */
    uint32_t mem_addr_len,  /* 地址字节数（1-4）*/
    void *data,             /* 接收缓冲区 */
    uint32_t len,           /* 请求读取字节数 */
    uint32_t max_len);      /* 最大允许字节数 */
```

内部时序：`START + addr(W) + mem_addr + repeated START + addr(R) + data + STOP`

---

## 示例

### 基本 I2C 读写

```c
#include "elib_simbus.h"

/* 假设的 GPIO 回调实现 */
static void gpio_write(uint8_t pin, uint8_t level) { /* ... */ }
static uint8_t gpio_read(uint8_t pin) { /* ... */ return 0; }
static void gpio_setdir(uint8_t pin, uint8_t dir) { /* ... */ }
static void delay_us(uint32_t us) { /* 硬件延时或空转 */ }

void example(void)
{
    elib_simbus_i2c_ctx_t ctx;
    uint8_t tx_buf[] = {0x12, 0x34, 0x56};
    uint8_t rx_buf[4] = {0};

    /* 初始化：SCL=pin0, SDA=pin1, 5µs 半周期, max_wait=0(默认100) */
    elib_simbus_i2c_init(&ctx, 0, 1, 5, 0,
        gpio_write, gpio_read, gpio_setdir, delay_us);

    /* 向从机 0x50 写 3 字节 */
    elib_simbus_i2c_write(&ctx, 0x50, tx_buf, 3, 3);

    /* 从从机 0x50 读 3 字节 */
    elib_simbus_i2c_read(&ctx, 0x50, rx_buf, 3, 3);

    /* 写寄存器：dev=0x50, reg=0x1234(2字节), 数据 4 字节 */
    uint8_t reg_data[] = {0xDE, 0xAD, 0xBE, 0xEF};
    elib_simbus_i2c_write_mem(&ctx, 0x50, 0x1234, 2, reg_data, 4, 4);

    /* 读寄存器 */
    uint8_t reg_rx[4] = {0};
    elib_simbus_i2c_read_mem(&ctx, 0x50, 0x1234, 2, reg_rx, 4, 4);

    elib_simbus_i2c_deinit(&ctx);
}
```

### 错误处理

```c
elib_simbus_err_t err;
uint8_t d = 0x55;

err = elib_simbus_i2c_write(&ctx, 0x50, &d, 1, 1);
switch (err) {
case ELIB_SIMBUS_OK:
    /* 成功 */
    break;
case ELIB_SIMBUS_ERR_NACK:
    /* 从机无应答（地址错误或设备忙） */
    break;
case ELIB_SIMBUS_ERR_TIMEOUT:
    /* 总线挂起（SCL 被从机长时间拉低等） */
    break;
case ELIB_SIMBUS_ERR_INVALID_PARAM:
    /* 参数错误 */
    break;
default:
    break;
}
```

### 定制超时

```c
/* max_wait=500：给复杂的多字节操作更多余量 */
elib_simbus_i2c_init(&ctx, 0, 1, 5, 500,
    gpio_write, gpio_read, gpio_setdir, delay_us);

/* 之后检查超时 */
elib_simbus_err_t err = elib_simbus_i2c_write(&ctx, 0x50, data, 32, 32);
if (err == ELIB_SIMBUS_ERR_TIMEOUT) {
    /* 32 字节写入在 500 轮内未完成，总线可能挂起 */
}
```

---

## 时序说明

### 时钟产生

`delay_us(delay_num)` 定义了 SCL 半周期：

```
SCL:  __|‾‾|__|‾‾|__|‾‾|__|‾‾|__
      delay  delay  delay  delay
```

对于 100kHz I2C：半周期 = 5µs，`delay_num = 5`。
对于 400kHz I2C：半周期 ≈ 1.3µs，`delay_num = 1`。

### 超时机制

超时仅作用于 **等待从机 ACK** 阶段。发送完每个字节（地址或数据）后，主机释放 SDA、拉高 SCL，然后轮询 SDA 引脚：

```
wait_rounds = 0
loop:
  read SDA
  if SDA == 0 → ACK 收到，继续
  if wait_rounds >= max_wait → 超时，置 timeout 标志，退出
  delay_us(delay_num)
  wait_rounds++
```

即每个字节的 ACK 阶段最多等待 `max_wait` 个 `delay_num` 周期。若从机始终不应答（如总线上无设备），超时后返回 `ELIB_SIMBUS_ERR_TIMEOUT`。

默认 `max_wait = 100`，大部分从机应在 1-2 轮内回复 ACK。
