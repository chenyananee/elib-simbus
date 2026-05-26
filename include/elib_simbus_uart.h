/* elib_simbus_uart.h - SimBus UART (Bit-Bang) */

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
typedef void (*elib_simbus_uart_io_setdir_t)(uint8_t pin, uint8_t dir);
typedef void (*elib_simbus_uart_delay_us_t)(uint32_t us);

/* ------------------------------------------------------------------ */
/*  Configuration                                                      */
/* ------------------------------------------------------------------ */

typedef struct {
    uint8_t  tx_pin;
    uint8_t  rx_pin;
    uint32_t bit_time_us;       /* 位时间 (µs) = 1000000 / baud */
    uint32_t data_bits;         /* 5-9, 默认 8 */
    uint32_t parity;            /* 0=none, 1=odd, 2=even */
    uint32_t stop_bits;         /* 1 或 2 */
    uint32_t timeout_rounds;    /* RX 起始位检测超时轮数，0=默认 10000 */

    elib_simbus_uart_io_write_t  io_write;
    elib_simbus_uart_io_read_t   io_read;
    elib_simbus_uart_io_setdir_t io_setdir;
    elib_simbus_uart_delay_us_t  delay_us;
} elib_simbus_uart_cfg_t;

/* ------------------------------------------------------------------ */
/*  Context                                                            */
/* ------------------------------------------------------------------ */

typedef struct {
    uint8_t  tx_pin;
    uint8_t  rx_pin;
    uint32_t bit_time_us;
    uint32_t data_bits;
    uint32_t parity;
    uint32_t stop_bits;
    uint32_t timeout_rounds;
    uint32_t wait_rounds;

    elib_simbus_uart_io_write_t  io_write;
    elib_simbus_uart_io_read_t   io_read;
    elib_simbus_uart_io_setdir_t io_setdir;
    elib_simbus_uart_delay_us_t  delay_us;

    struct {
        uint8_t initialized : 1;
        uint8_t timeout     : 1;
        uint8_t reserved    : 6;
    } bit_flags;
} elib_simbus_uart_ctx_t;

/* ------------------------------------------------------------------ */
/*  API                                                                */
/* ------------------------------------------------------------------ */

elib_simbus_err_t elib_simbus_uart_init(
    elib_simbus_uart_ctx_t *ctx,
    const elib_simbus_uart_cfg_t *cfg);

void elib_simbus_uart_deinit(elib_simbus_uart_ctx_t *ctx);

/**
 * @brief Send one byte
 */
elib_simbus_err_t elib_simbus_uart_putchar(
    elib_simbus_uart_ctx_t *ctx, uint8_t byte);

/**
 * @brief Receive one byte with timeout
 * @return 0-255 on success, -1 on timeout
 */
int32_t elib_simbus_uart_getchar(
    elib_simbus_uart_ctx_t *ctx);

/**
 * @brief Send multiple bytes
 */
elib_simbus_err_t elib_simbus_uart_write(
    elib_simbus_uart_ctx_t *ctx,
    const void *data,
    uint32_t len,
    uint32_t max_len);

/**
 * @brief Receive multiple bytes
 * @return Number of bytes received, or negative on error
 */
int32_t elib_simbus_uart_read(
    elib_simbus_uart_ctx_t *ctx,
    void *data,
    uint32_t len,
    uint32_t max_len);

#ifdef __cplusplus
}
#endif

#endif /* ELIB_SIMBUS_UART_H */
