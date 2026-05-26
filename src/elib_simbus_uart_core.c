/* elib_simbus_uart_core.c - SimBus UART Bit-Bang Implementation */

#include "elib_simbus_uart_core.h"
#include <stddef.h>

#define ELIB_SIMBUS_UART_DEFAULT_TIMEOUT_ROUNDS 10000
#define ELIB_SIMBUS_UART_DEFAULT_DATA_BITS 8
#define ELIB_SIMBUS_UART_DEFAULT_STOP_BITS 1

/* ------------------------------------------------------------------ */
/*  Internal helpers                                                    */
/* ------------------------------------------------------------------ */

static inline void uart_delay(elib_simbus_uart_ctx_t *ctx)
{
    ctx->delay_us(ctx->bit_time_us);
}

static inline void uart_tx_low(elib_simbus_uart_ctx_t *ctx)
{
    ctx->io_write(ctx->tx_pin, 0);
    ctx->io_setdir(ctx->tx_pin, 1);
}

static inline void uart_tx_high(elib_simbus_uart_ctx_t *ctx)
{
    ctx->io_write(ctx->tx_pin, 1);
    ctx->io_setdir(ctx->tx_pin, 1);
}

static uint8_t calc_parity_bit(uint8_t byte, uint32_t data_bits, uint32_t parity_mode)
{
    uint8_t p = 0;
    for (uint32_t i = 0; i < data_bits; i++) {
        p ^= (byte >> i) & 1;
    }
    /* parity_mode: 1=odd, 2=even */
    if (parity_mode == 2) return p;      /* even: parity = XOR */
    return (uint8_t)(p ^ 1);              /* odd:  parity = !XOR */
}

/* ------------------------------------------------------------------ */
/*  Transmit                                                           */
/* ------------------------------------------------------------------ */

static void uart_send_byte(elib_simbus_uart_ctx_t *ctx, uint8_t byte)
{
    uint32_t total_bits = ctx->data_bits;
    uint8_t mask = (uint8_t)((total_bits == 9) ? 0xFF : ((1U << total_bits) - 1));
    byte &= mask;

    /* Start bit: low */
    uart_tx_low(ctx);
    uart_delay(ctx);

    /* Data bits (LSB first) */
    for (uint32_t i = 0; i < total_bits; i++) {
        if (byte & (1U << i)) {
            uart_tx_high(ctx);
        } else {
            uart_tx_low(ctx);
        }
        uart_delay(ctx);
    }

    /* Parity bit */
    if (ctx->parity) {
        uint8_t parity_bit = calc_parity_bit(byte, total_bits, ctx->parity);
        if (parity_bit) {
            uart_tx_high(ctx);
        } else {
            uart_tx_low(ctx);
        }
        uart_delay(ctx);
    }

    /* Stop bit(s): high */
    for (uint32_t i = 0; i < ctx->stop_bits; i++) {
        uart_tx_high(ctx);
        uart_delay(ctx);
    }
}

/* ------------------------------------------------------------------ */
/*  Receive                                                            */
/* ------------------------------------------------------------------ */

static int32_t uart_recv_byte(elib_simbus_uart_ctx_t *ctx)
{
    uint32_t total_bits = ctx->data_bits;

    /* Wait for start bit (RX falling edge) */
    uint8_t prev_rx = ctx->io_read(ctx->rx_pin);
    ctx->wait_rounds = 0;

    for (;;) {
        uint8_t rx = ctx->io_read(ctx->rx_pin);
        if (prev_rx == 1 && rx == 0) {
            break;  /* start bit detected */
        }
        prev_rx = rx;
        if (ctx->wait_rounds >= ctx->timeout_rounds) {
            ctx->bit_flags.timeout = 1;
            return -1;
        }
        ctx->delay_us(ctx->bit_time_us);
        ctx->wait_rounds++;
    }

    /* Sample in middle of start bit */
    ctx->delay_us(ctx->bit_time_us / 2);

    /* Data bits (LSB first) */
    uint8_t byte = 0;
    for (uint32_t i = 0; i < total_bits; i++) {
        ctx->delay_us(ctx->bit_time_us);
        if (ctx->io_read(ctx->rx_pin)) {
            byte |= (uint8_t)(1U << i);
        }
    }

    /* Parity bit (skip validation, just consume) */
    if (ctx->parity) {
        ctx->delay_us(ctx->bit_time_us);
    }

    /* Stop bit(s) */
    for (uint32_t i = 0; i < ctx->stop_bits; i++) {
        ctx->delay_us(ctx->bit_time_us);
    }

    return (int32_t)byte;
}

/* ------------------------------------------------------------------ */
/*  Public API                                                          */
/* ------------------------------------------------------------------ */

elib_simbus_err_t elib_simbus_uart_init(
    elib_simbus_uart_ctx_t *ctx,
    const elib_simbus_uart_cfg_t *cfg)
{
    if (ctx == NULL || cfg == NULL ||
        cfg->io_write == NULL || cfg->io_read == NULL ||
        cfg->io_setdir == NULL || cfg->delay_us == NULL) {
        return ELIB_SIMBUS_ERR_INVALID_PARAM;
    }
    if (cfg->bit_time_us == 0) {
        return ELIB_SIMBUS_ERR_INVALID_PARAM;
    }
    if (cfg->data_bits != 0 && (cfg->data_bits < 5 || cfg->data_bits > 9)) {
        return ELIB_SIMBUS_ERR_INVALID_PARAM;
    }
    if (cfg->parity > 2) {
        return ELIB_SIMBUS_ERR_INVALID_PARAM;
    }
    if (cfg->stop_bits > 2) {
        return ELIB_SIMBUS_ERR_INVALID_PARAM;
    }

    ctx->tx_pin    = cfg->tx_pin;
    ctx->rx_pin    = cfg->rx_pin;
    ctx->bit_time_us = cfg->bit_time_us;
    ctx->data_bits = (cfg->data_bits == 0) ? ELIB_SIMBUS_UART_DEFAULT_DATA_BITS : cfg->data_bits;
    ctx->parity    = cfg->parity;
    ctx->stop_bits = (cfg->stop_bits == 0) ? ELIB_SIMBUS_UART_DEFAULT_STOP_BITS : cfg->stop_bits;
    ctx->timeout_rounds = (cfg->timeout_rounds == 0) ? ELIB_SIMBUS_UART_DEFAULT_TIMEOUT_ROUNDS : cfg->timeout_rounds;
    ctx->io_write  = cfg->io_write;
    ctx->io_read   = cfg->io_read;
    ctx->io_setdir = cfg->io_setdir;
    ctx->delay_us  = cfg->delay_us;
    ctx->bit_flags.initialized = 1;
    ctx->bit_flags.timeout     = 0;

    return ELIB_SIMBUS_OK;
}

void elib_simbus_uart_deinit(elib_simbus_uart_ctx_t *ctx)
{
    if (ctx == NULL) return;
    ctx->bit_flags.initialized = 0;
}

elib_simbus_err_t elib_simbus_uart_putchar(
    elib_simbus_uart_ctx_t *ctx, uint8_t byte)
{
    if (ctx == NULL) return ELIB_SIMBUS_ERR_INVALID_PARAM;
    if (!ctx->bit_flags.initialized) return ELIB_SIMBUS_ERR_NOT_INITIALIZED;

    uart_send_byte(ctx, byte);
    return ELIB_SIMBUS_OK;
}

int32_t elib_simbus_uart_getchar(elib_simbus_uart_ctx_t *ctx)
{
    if (ctx == NULL || !ctx->bit_flags.initialized) return -1;

    ctx->bit_flags.timeout = 0;
    ctx->wait_rounds = 0;

    return uart_recv_byte(ctx);
}

elib_simbus_err_t elib_simbus_uart_write(
    elib_simbus_uart_ctx_t *ctx,
    const void *data,
    uint32_t len,
    uint32_t max_len)
{
    if (ctx == NULL || data == NULL) return ELIB_SIMBUS_ERR_INVALID_PARAM;
    if (!ctx->bit_flags.initialized) return ELIB_SIMBUS_ERR_NOT_INITIALIZED;
    if (len > max_len) return ELIB_SIMBUS_ERR_EXCEED_MAX;
    if (len == 0) return ELIB_SIMBUS_OK;

    const uint8_t *buf = (const uint8_t *)data;
    for (uint32_t i = 0; i < len; i++) {
        uart_send_byte(ctx, buf[i]);
    }

    return ELIB_SIMBUS_OK;
}

int32_t elib_simbus_uart_read(
    elib_simbus_uart_ctx_t *ctx,
    void *data,
    uint32_t len,
    uint32_t max_len)
{
    if (ctx == NULL || data == NULL) return -1;
    if (!ctx->bit_flags.initialized) return -1;
    if (len > max_len) return -1;
    if (len == 0) return 0;

    uint8_t *buf = (uint8_t *)data;
    uint32_t received = 0;

    for (uint32_t i = 0; i < len; i++) {
        int32_t byte = elib_simbus_uart_getchar(ctx);
        if (byte < 0) break;
        buf[i] = (uint8_t)byte;
        received++;
    }

    return (int32_t)received;
}
