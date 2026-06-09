/* elib_simbus_uart_core.c - SimBus UART Bit-Bang State Machine */

#include "elib_simbus_uart_core.h"
#include <stddef.h>

#define ELIB_SIMBUS_UART_DEFAULT_DATA_BITS  8
#define ELIB_SIMBUS_UART_DEFAULT_STOP_BITS  1

/* ------------------------------------------------------------------ */
/*  Internal helpers                                                    */
/* ------------------------------------------------------------------ */

static uint8_t calc_parity_bit(uint8_t byte, uint32_t data_bits, uint32_t parity_mode)
{
    uint8_t p = 0;
    for (uint32_t i = 0; i < data_bits; i++) {
        p ^= (byte >> i) & 1;
    }
    if (parity_mode == 2) return p;          /* even */
    return (uint8_t)(p ^ 1);                  /* odd  */
}

static inline void tx_drive(elib_simbus_uart_ctx_t *ctx, uint8_t level)
{
    ctx->io_write(ctx->tx_pin, level);
}

/* Load byte into TX shift register and emit start bit */
static void tx_load_byte(elib_simbus_uart_ctx_t *ctx, uint8_t byte)
{
    uint32_t total_bits = ctx->data_bits;
    uint8_t mask = (uint8_t)((total_bits == 9) ? 0xFF : ((1U << total_bits) - 1));
    ctx->tx_byte = byte & mask;
    ctx->tx_bit_idx = 0;
    ctx->tx_acc_ns = 0;

    if (ctx->parity) {
        ctx->tx_parity_val = calc_parity_bit(ctx->tx_byte, total_bits, ctx->parity);
    }

    tx_drive(ctx, 0);
    ctx->tx_state = ELIB_SIMBUS_UART_STATE_START_BIT;
}

/* ------------------------------------------------------------------ */
/*  Public API                                                          */
/* ------------------------------------------------------------------ */

elib_simbus_err_t elib_simbus_uart_init(
    elib_simbus_uart_ctx_t *ctx,
    const elib_simbus_uart_cfg_t *cfg)
{
    if (ctx == NULL || cfg == NULL ||
        cfg->io_write == NULL || cfg->io_read == NULL) {
        return ELIB_SIMBUS_ERR_INVALID_PARAM;
    }
    if (cfg->bit_time_ns == 0) {
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

    ctx->tx_pin      = cfg->tx_pin;
    ctx->rx_pin      = cfg->rx_pin;
    ctx->bit_time_ns = cfg->bit_time_ns;
    ctx->data_bits   = (cfg->data_bits == 0) ? ELIB_SIMBUS_UART_DEFAULT_DATA_BITS : cfg->data_bits;
    ctx->parity      = cfg->parity;
    ctx->stop_bits   = (cfg->stop_bits == 0) ? ELIB_SIMBUS_UART_DEFAULT_STOP_BITS : cfg->stop_bits;

    ctx->io_write    = cfg->io_write;
    ctx->io_read     = cfg->io_read;
    ctx->rx_callback = cfg->rx_callback;

    /* TX state machine */
    ctx->tx_state     = ELIB_SIMBUS_UART_STATE_IDLE;
    ctx->tx_byte      = 0;
    ctx->tx_bit_idx   = 0;
    ctx->tx_parity_val = 0;
    ctx->tx_acc_ns    = 0;
    ctx->tx_buf       = NULL;
    ctx->tx_buf_len   = 0;
    ctx->tx_buf_idx   = 0;

    /* RX state machine */
    ctx->rx_state     = ELIB_SIMBUS_UART_STATE_IDLE;
    ctx->rx_frame     = 0;
    ctx->rx_bit_idx   = 0;
    ctx->rx_prev_level = ctx->io_read(ctx->rx_pin);
    ctx->rx_acc_ns    = 0;

    ctx->bit_flags.initialized = 1;

    return ELIB_SIMBUS_OK;
}

void elib_simbus_uart_deinit(elib_simbus_uart_ctx_t *ctx)
{
    if (ctx == NULL) return;
    ctx->bit_flags.initialized = 0;
    ctx->tx_state = ELIB_SIMBUS_UART_STATE_IDLE;
    ctx->rx_state = ELIB_SIMBUS_UART_STATE_IDLE;
}

elib_simbus_err_t elib_simbus_uart_start_tx(
    elib_simbus_uart_ctx_t *ctx, const uint8_t *data, uint32_t len)
{
    if (ctx == NULL || data == NULL || len == 0) {
        return ELIB_SIMBUS_ERR_INVALID_PARAM;
    }
    if (!ctx->bit_flags.initialized) return ELIB_SIMBUS_ERR_NOT_INITIALIZED;
    if (ctx->tx_state != ELIB_SIMBUS_UART_STATE_IDLE) {
        return ELIB_SIMBUS_ERR_INVALID_PARAM; /* TX busy */
    }

    ctx->tx_buf = data;
    ctx->tx_buf_len = len;
    ctx->tx_buf_idx = 0;
    tx_load_byte(ctx, data[0]);

    return ELIB_SIMBUS_OK;
}

void elib_simbus_uart_poll_tx(
    elib_simbus_uart_ctx_t *ctx, uint32_t elapsed_ns)
{
    if (ctx == NULL || !ctx->bit_flags.initialized) return;
    if (ctx->tx_state == ELIB_SIMBUS_UART_STATE_IDLE) return;

    uint32_t bit_ns = ctx->bit_time_ns;
    ctx->tx_acc_ns += elapsed_ns;
    if (ctx->tx_acc_ns < bit_ns) return;

    switch (ctx->tx_state) {
    case ELIB_SIMBUS_UART_STATE_START_BIT:
        ctx->tx_acc_ns -= bit_ns;
        tx_drive(ctx, (ctx->tx_byte >> 0) & 1);
        ctx->tx_bit_idx = 0;
        ctx->tx_state = ELIB_SIMBUS_UART_STATE_DATA_BIT;
        break;

    case ELIB_SIMBUS_UART_STATE_DATA_BIT:
        ctx->tx_acc_ns -= bit_ns;
        ctx->tx_bit_idx++;
        if (ctx->tx_bit_idx < ctx->data_bits) {
            tx_drive(ctx, (ctx->tx_byte >> ctx->tx_bit_idx) & 1);
        } else if (ctx->parity) {
            tx_drive(ctx, ctx->tx_parity_val);
            ctx->tx_state = ELIB_SIMBUS_UART_STATE_PARITY_BIT;
        } else {
            tx_drive(ctx, 1);
            ctx->tx_bit_idx = 0;
            ctx->tx_state = ELIB_SIMBUS_UART_STATE_STOP_BIT;
        }
        break;

    case ELIB_SIMBUS_UART_STATE_PARITY_BIT:
        ctx->tx_acc_ns -= bit_ns;
        tx_drive(ctx, 1);
        ctx->tx_bit_idx = 0;
        ctx->tx_state = ELIB_SIMBUS_UART_STATE_STOP_BIT;
        break;

    case ELIB_SIMBUS_UART_STATE_STOP_BIT:
        ctx->tx_bit_idx++;
        if (ctx->tx_bit_idx < ctx->stop_bits) {
            ctx->tx_acc_ns -= bit_ns;
            tx_drive(ctx, 1);
        } else if (ctx->tx_buf_idx + 1 < ctx->tx_buf_len) {
            /* More bytes: chain to next */
            ctx->tx_buf_idx++;
            tx_load_byte(ctx, ctx->tx_buf[ctx->tx_buf_idx]);
        } else {
            /* Last byte done: drive idle */
            ctx->tx_acc_ns -= bit_ns;
            tx_drive(ctx, 1);
            ctx->tx_state = ELIB_SIMBUS_UART_STATE_IDLE;
        }
        break;

    default:
        ctx->tx_state = ELIB_SIMBUS_UART_STATE_IDLE;
        ctx->tx_acc_ns = 0;
        break;
    }
}

void elib_simbus_uart_poll_rx(
    elib_simbus_uart_ctx_t *ctx, uint32_t elapsed_ns)
{
    if (ctx == NULL || !ctx->bit_flags.initialized) return;

    uint32_t bit_ns = ctx->bit_time_ns;

    if (ctx->rx_state == ELIB_SIMBUS_UART_STATE_IDLE) {
        /* Idle: sample RX line for falling edge (start bit) */
        uint8_t level = ctx->io_read(ctx->rx_pin);
        if (ctx->rx_prev_level == 1 && level == 0) {
            /* Falling edge detected, begin start bit.
             * Pre-load accumulator to half bit time so first transition
             * occurs at 0.5 bit times (start-bit center), giving
             * sampling at 1.5, 2.5, 3.5... (bit centers). */
            ctx->rx_state = ELIB_SIMBUS_UART_STATE_START_BIT;
            ctx->rx_acc_ns = bit_ns / 2;
            ctx->rx_frame = 0;
            ctx->rx_bit_idx = 0;
        }
        ctx->rx_prev_level = level;
        return;
    }

    ctx->rx_acc_ns += elapsed_ns;
    if (ctx->rx_acc_ns < bit_ns) return;
    ctx->rx_acc_ns -= bit_ns;

    switch (ctx->rx_state) {
    case ELIB_SIMBUS_UART_STATE_START_BIT:
        /* Half-bit delay consumed during edge detection.
         * Now at start-bit center; advance to first data bit. */
        ctx->rx_state = ELIB_SIMBUS_UART_STATE_DATA_BIT;
        ctx->rx_bit_idx = 0;
        break;

    case ELIB_SIMBUS_UART_STATE_DATA_BIT:
        if (ctx->io_read(ctx->rx_pin)) {
            ctx->rx_frame |= (uint8_t)(1U << ctx->rx_bit_idx);
        }
        ctx->rx_bit_idx++;
        if (ctx->rx_bit_idx >= ctx->data_bits) {
            if (ctx->parity) {
                ctx->rx_state = ELIB_SIMBUS_UART_STATE_PARITY_BIT;
            } else {
                ctx->rx_state = ELIB_SIMBUS_UART_STATE_STOP_BIT;
                ctx->rx_bit_idx = 0;
            }
        }
        break;

    case ELIB_SIMBUS_UART_STATE_PARITY_BIT:
        /* Skip parity validation, just consume */
        ctx->rx_state = ELIB_SIMBUS_UART_STATE_STOP_BIT;
        ctx->rx_bit_idx = 0;
        break;

    case ELIB_SIMBUS_UART_STATE_STOP_BIT:
        ctx->rx_bit_idx++;
        if (ctx->rx_bit_idx >= ctx->stop_bits) {
            /* Byte received, invoke callback */
            if (ctx->rx_callback) {
                ctx->rx_callback(ctx, ctx->rx_frame);
            }
            ctx->rx_state = ELIB_SIMBUS_UART_STATE_IDLE;
            ctx->rx_prev_level = ctx->io_read(ctx->rx_pin);
            ctx->rx_acc_ns = 0;
        }
        break;

    default:
        ctx->rx_state = ELIB_SIMBUS_UART_STATE_IDLE;
        ctx->rx_acc_ns = 0;
        break;
    }
}

uint8_t elib_simbus_uart_tx_busy(elib_simbus_uart_ctx_t *ctx)
{
    if (ctx == NULL) return 0;
    return (ctx->tx_state != ELIB_SIMBUS_UART_STATE_IDLE) ? 1 : 0;
}
