/* elib_simbus_uart.h - SimBus UART (Bit-Bang, State-Machine Driven) */

#ifndef ELIB_SIMBUS_UART_H
#define ELIB_SIMBUS_UART_H

#include <stdint.h>
#include "elib_simbus_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ------------------------------------------------------------------ */
/*  Callback type definitions                                          */
/* ------------------------------------------------------------------ */

typedef void (*elib_simbus_uart_io_write_t)(uint8_t pin, uint8_t level);
typedef uint8_t (*elib_simbus_uart_io_read_t)(uint8_t pin);

/**
 * @brief RX byte received callback
 * @param ctx  UART context
 * @param byte Received byte
 */
typedef void (*elib_simbus_uart_rx_callback_t)(
    struct elib_simbus_uart_ctx *ctx, uint8_t byte);

/* ------------------------------------------------------------------ */
/*  State enums                                                        */
/* ------------------------------------------------------------------ */

typedef enum {
    ELIB_SIMBUS_UART_STATE_IDLE = 0,
    ELIB_SIMBUS_UART_STATE_START_BIT,
    ELIB_SIMBUS_UART_STATE_DATA_BIT,
    ELIB_SIMBUS_UART_STATE_PARITY_BIT,
    ELIB_SIMBUS_UART_STATE_STOP_BIT,
} elib_simbus_uart_state_t;

/* ------------------------------------------------------------------ */
/*  Configuration                                                      */
/* ------------------------------------------------------------------ */

typedef struct {
    uint8_t  tx_pin;
    uint8_t  rx_pin;
    uint32_t bit_time_ns;       /* 位时间 (ns) = 1000000000 / baud */
    uint32_t data_bits;         /* 5-9, 默认 8 */
    uint32_t parity;            /* 0=none, 1=odd, 2=even */
    uint32_t stop_bits;         /* 1 或 2 */

    elib_simbus_uart_io_write_t  io_write;
    elib_simbus_uart_io_read_t   io_read;
    elib_simbus_uart_rx_callback_t rx_callback;
} elib_simbus_uart_cfg_t;

/* ------------------------------------------------------------------ */
/*  Context                                                            */
/* ------------------------------------------------------------------ */

typedef struct elib_simbus_uart_ctx {
    uint8_t  tx_pin;
    uint8_t  rx_pin;
    uint32_t bit_time_ns;
    uint32_t data_bits;
    uint32_t parity;
    uint32_t stop_bits;

    elib_simbus_uart_io_write_t  io_write;
    elib_simbus_uart_io_read_t   io_read;
    elib_simbus_uart_rx_callback_t rx_callback;

    /* TX state machine */
    elib_simbus_uart_state_t tx_state;
    uint8_t  tx_byte;           /* 当前发送的字节 */
    uint8_t  tx_bit_idx;        /* 当前数据位索引 / 停止位索引 */
    uint8_t  tx_parity_val;     /* 校验位值 */
    uint32_t tx_acc_ns;         /* TX 时间累加器 */
    const uint8_t *tx_buf;      /* 连续发送缓冲区 (NULL=单字节模式) */
    uint32_t tx_buf_len;        /* 缓冲区总长度 */
    uint32_t tx_buf_idx;        /* 当前发送位置 */

    /* RX state machine */
    elib_simbus_uart_state_t rx_state;
    uint8_t  rx_frame;          /* 接收中的字节 */
    uint8_t  rx_bit_idx;        /* 当前数据位索引 */
    uint8_t  rx_prev_level;     /* 上次 RX 引脚电平 */
    uint32_t rx_acc_ns;         /* RX 时间累加器 */

    struct {
        uint8_t initialized : 1;
        uint8_t reserved    : 7;
    } bit_flags;
} elib_simbus_uart_ctx_t;

/* ------------------------------------------------------------------ */
/*  API                                                                */
/* ------------------------------------------------------------------ */

/**
 * @brief Initialize UART context
 */
elib_simbus_err_t elib_simbus_uart_init(
    elib_simbus_uart_ctx_t *ctx,
    const elib_simbus_uart_cfg_t *cfg);

/**
 * @brief Deinitialize UART context
 */
void elib_simbus_uart_deinit(elib_simbus_uart_ctx_t *ctx);

/**
 * @brief Start transmitting bytes
 * @param ctx  UART context
 * @param data Pointer to byte array
 * @param len  Number of bytes to send (1 = single byte)
 * @return ELIB_SIMBUS_ERR_INVALID_PARAM if TX is busy or invalid param
 */
elib_simbus_err_t elib_simbus_uart_start_tx(
    elib_simbus_uart_ctx_t *ctx, const uint8_t *data, uint32_t len);

/**
 * @brief Poll TX state machine, advancing at most one bit
 * @param ctx        UART context
 * @param elapsed_ns Nanoseconds since last poll
 */
void elib_simbus_uart_poll_tx(
    elib_simbus_uart_ctx_t *ctx, uint32_t elapsed_ns);

/**
 * @brief Poll RX state machine, advancing at most one bit
 * @param ctx        UART context
 * @param elapsed_ns Nanoseconds since last poll
 */
void elib_simbus_uart_poll_rx(
    elib_simbus_uart_ctx_t *ctx, uint32_t elapsed_ns);

/**
 * @brief Check if TX is in progress
 * @return 1 if busy, 0 if idle
 */
uint8_t elib_simbus_uart_tx_busy(elib_simbus_uart_ctx_t *ctx);

#ifdef __cplusplus
}
#endif

#endif /* ELIB_SIMBUS_UART_H */
