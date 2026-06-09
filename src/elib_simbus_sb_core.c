/* elib_simbus_sb_core.c - SimBus Single-Bus Protocol State Machine */

#include "elib_simbus_sb_core.h"
#include <stddef.h>

#define SB_IDLE_CHECK_BITS 15

/* ------------------------------------------------------------------ */
/*  Internal helpers                                                    */
/* ------------------------------------------------------------------ */

static uint8_t calc_parity4(uint8_t byte)
{
    uint8_t lo = byte & 0x0F;
    uint8_t hi = (byte >> 4) & 0x0F;
    uint8_t p0 = 0, p1 = 0, p2, p3;
    for (int i = 0; i < 4; i++) {
        p0 ^= (lo >> i) & 1;
        p1 ^= (hi >> i) & 1;
    }
    p2 = ((byte>>0)&1) ^ ((byte>>4)&1) ^ ((byte>>1)&1) ^ ((byte>>5)&1);
    p3 = ((byte>>2)&1) ^ ((byte>>6)&1) ^ ((byte>>3)&1) ^ ((byte>>7)&1);
    return (uint8_t)((p3<<3) | (p2<<2) | (p1<<1) | p0);
}

static inline void sb_drive(elib_simbus_sb_ctx_t *ctx, uint8_t level)
{
    ctx->io_setdir(ctx->pin, ELIB_SIMBUS_SB_DIR_OUTPUT);
    ctx->io_write(ctx->pin, level);
}

static inline void sb_release(elib_simbus_sb_ctx_t *ctx)
{
    ctx->io_setdir(ctx->pin, ELIB_SIMBUS_SB_DIR_INPUT);
}

/* ------------------------------------------------------------------ */
/*  Public API                                                          */
/* ------------------------------------------------------------------ */

elib_simbus_err_t elib_simbus_sb_init(
    elib_simbus_sb_ctx_t *ctx,
    const elib_simbus_sb_cfg_t *cfg)
{
    if (ctx == NULL || cfg == NULL ||
        cfg->io_write == NULL || cfg->io_read == NULL ||
        cfg->io_setdir == NULL) {
        return ELIB_SIMBUS_ERR_INVALID_PARAM;
    }
    if (cfg->bit_time_ns == 0) {
        return ELIB_SIMBUS_ERR_INVALID_PARAM;
    }

    ctx->pin         = cfg->pin;
    ctx->bit_time_ns = cfg->bit_time_ns;
    ctx->io_write    = cfg->io_write;
    ctx->io_read     = cfg->io_read;
    ctx->io_setdir   = cfg->io_setdir;
    ctx->rx_callback = cfg->rx_callback;

    ctx->io_setdir(ctx->pin, ELIB_SIMBUS_SB_DIR_INPUT);

    ctx->state    = ELIB_SIMBUS_SB_IDLE;
    ctx->byte     = 0;
    ctx->parity   = 0;
    ctx->bit_idx  = 0;
    ctx->acc_ns   = 0;
    ctx->prev_level = ctx->io_read(ctx->pin);

    ctx->tx_buf   = NULL;
    ctx->tx_buf_len = 0;
    ctx->tx_buf_idx = 0;
    ctx->tx_idle_countdown = 0;

    ctx->bit_flags.initialized = 1;

    return ELIB_SIMBUS_OK;
}

void elib_simbus_sb_deinit(elib_simbus_sb_ctx_t *ctx)
{
    if (ctx == NULL) return;
    ctx->bit_flags.initialized = 0;
    ctx->io_setdir(ctx->pin, ELIB_SIMBUS_SB_DIR_INPUT);
    ctx->state = ELIB_SIMBUS_SB_IDLE;
}

elib_simbus_err_t elib_simbus_sb_start_tx(
    elib_simbus_sb_ctx_t *ctx, const uint8_t *data, uint32_t len)
{
    if (ctx == NULL || data == NULL || len == 0) {
        return ELIB_SIMBUS_ERR_INVALID_PARAM;
    }
    if (!ctx->bit_flags.initialized) return ELIB_SIMBUS_ERR_NOT_INITIALIZED;
    if (ctx->state != ELIB_SIMBUS_SB_IDLE) {
        return ELIB_SIMBUS_ERR_INVALID_PARAM; /* busy (TX or RX in progress) */
    }

    ctx->tx_buf = data;
    ctx->tx_buf_len = len;
    ctx->tx_buf_idx = 0;
    ctx->acc_ns = 0;
    ctx->tx_idle_countdown = (int32_t)SB_IDLE_CHECK_BITS * ctx->bit_time_ns;
    ctx->state = ELIB_SIMBUS_SB_TX_IDLE_CHECK;

    return ELIB_SIMBUS_OK;
}

void elib_simbus_sb_poll(
    elib_simbus_sb_ctx_t *ctx, uint32_t elapsed_ns)
{
    if (ctx == NULL || !ctx->bit_flags.initialized) return;

    uint32_t bit_ns = ctx->bit_time_ns;

    switch (ctx->state) {

    /* ============================================================ */
    /*  IDLE: listen for RX falling edge, or accept TX start         */
    /* ============================================================ */
    case ELIB_SIMBUS_SB_IDLE: {
        uint8_t level = ctx->io_read(ctx->pin);
        if (ctx->prev_level == 1 && level == 0) {
            /* Falling edge → start receiving */
            ctx->state = ELIB_SIMBUS_SB_RX_START_BIT;
            ctx->acc_ns = bit_ns / 2;
            ctx->byte = 0;
            ctx->parity = 0;
            ctx->bit_idx = 0;
        }
        ctx->prev_level = level;
        return;
    }

    /* ============================================================ */
    /*  TX: idle check                                               */
    /* ============================================================ */
    case ELIB_SIMBUS_SB_TX_IDLE_CHECK:
        if (ctx->io_read(ctx->pin) == 0) {
            ctx->tx_idle_countdown = (int32_t)SB_IDLE_CHECK_BITS * bit_ns;
            return;
        }
        ctx->tx_idle_countdown -= elapsed_ns;
        if (ctx->tx_idle_countdown > 0) return;

        /* Bus idle confirmed, load byte and emit start bit */
        ctx->byte = ctx->tx_buf[0];
        ctx->parity = calc_parity4(ctx->byte);
        ctx->bit_idx = 0;
        ctx->acc_ns = 0;
        sb_drive(ctx, 0);
        ctx->state = ELIB_SIMBUS_SB_TX_START_BIT;
        return;

    /* ============================================================ */
    /*  TX: frame bits                                               */
    /* ============================================================ */
    case ELIB_SIMBUS_SB_TX_START_BIT:
        ctx->acc_ns += elapsed_ns;
        if (ctx->acc_ns < bit_ns) return;
        ctx->acc_ns -= bit_ns;
        sb_drive(ctx, ctx->byte & 1);
        ctx->bit_idx = 0;
        ctx->state = ELIB_SIMBUS_SB_TX_DATA_BIT;
        return;

    case ELIB_SIMBUS_SB_TX_DATA_BIT:
        ctx->acc_ns += elapsed_ns;
        if (ctx->acc_ns < bit_ns) return;
        ctx->acc_ns -= bit_ns;
        ctx->bit_idx++;
        if (ctx->bit_idx < 8) {
            sb_drive(ctx, (ctx->byte >> ctx->bit_idx) & 1);
        } else {
            sb_drive(ctx, ctx->parity & 1);
            ctx->bit_idx = 0;
            ctx->state = ELIB_SIMBUS_SB_TX_PARITY_BIT;
        }
        return;

    case ELIB_SIMBUS_SB_TX_PARITY_BIT:
        ctx->acc_ns += elapsed_ns;
        if (ctx->acc_ns < bit_ns) return;
        ctx->acc_ns -= bit_ns;
        ctx->bit_idx++;
        if (ctx->bit_idx < 4) {
            sb_drive(ctx, (ctx->parity >> ctx->bit_idx) & 1);
        } else {
            sb_drive(ctx, 1);
            ctx->bit_idx = 0;
            ctx->state = ELIB_SIMBUS_SB_TX_STOP_BIT;
        }
        return;

    case ELIB_SIMBUS_SB_TX_STOP_BIT:
        ctx->acc_ns += elapsed_ns;
        if (ctx->acc_ns < bit_ns) return;
        if (ctx->tx_buf_idx + 1 < ctx->tx_buf_len) {
            /* Chain to next byte */
            ctx->tx_buf_idx++;
            ctx->tx_idle_countdown = (int32_t)SB_IDLE_CHECK_BITS * bit_ns;
            ctx->acc_ns = 0;
            ctx->state = ELIB_SIMBUS_SB_TX_IDLE_CHECK;
        } else {
            ctx->acc_ns -= bit_ns;
            sb_drive(ctx, 1);
            sb_release(ctx);
            ctx->state = ELIB_SIMBUS_SB_IDLE;
            ctx->prev_level = ctx->io_read(ctx->pin);
        }
        return;

    /* ============================================================ */
    /*  RX: frame bits                                               */
    /* ============================================================ */
    case ELIB_SIMBUS_SB_RX_START_BIT:
        ctx->acc_ns += elapsed_ns;
        if (ctx->acc_ns < bit_ns) return;
        ctx->acc_ns -= bit_ns;
        ctx->state = ELIB_SIMBUS_SB_RX_DATA_BIT;
        ctx->bit_idx = 0;
        return;

    case ELIB_SIMBUS_SB_RX_DATA_BIT:
        ctx->acc_ns += elapsed_ns;
        if (ctx->acc_ns < bit_ns) return;
        ctx->acc_ns -= bit_ns;
        if (ctx->io_read(ctx->pin)) {
            ctx->byte |= (uint8_t)(1U << ctx->bit_idx);
        }
        ctx->bit_idx++;
        if (ctx->bit_idx >= 8) {
            ctx->state = ELIB_SIMBUS_SB_RX_PARITY_BIT;
            ctx->bit_idx = 0;
        }
        return;

    case ELIB_SIMBUS_SB_RX_PARITY_BIT:
        ctx->acc_ns += elapsed_ns;
        if (ctx->acc_ns < bit_ns) return;
        ctx->acc_ns -= bit_ns;
        if (ctx->io_read(ctx->pin)) {
            ctx->parity |= (uint8_t)(1U << ctx->bit_idx);
        }
        ctx->bit_idx++;
        if (ctx->bit_idx >= 4) {
            ctx->state = ELIB_SIMBUS_SB_RX_STOP_BIT;
        }
        return;

    case ELIB_SIMBUS_SB_RX_STOP_BIT:
        ctx->acc_ns += elapsed_ns;
        if (ctx->acc_ns < bit_ns) return;
        if (ctx->rx_callback) {
            ctx->rx_callback(ctx, ctx->byte);
        }
        ctx->state = ELIB_SIMBUS_SB_IDLE;
        ctx->prev_level = ctx->io_read(ctx->pin);
        ctx->acc_ns = 0;
        return;

    default:
        sb_release(ctx);
        ctx->state = ELIB_SIMBUS_SB_IDLE;
        ctx->acc_ns = 0;
        return;
    }
}

uint8_t elib_simbus_sb_tx_busy(elib_simbus_sb_ctx_t *ctx)
{
    if (ctx == NULL) return 0;
    switch (ctx->state) {
    case ELIB_SIMBUS_SB_TX_IDLE_CHECK:
    case ELIB_SIMBUS_SB_TX_START_BIT:
    case ELIB_SIMBUS_SB_TX_DATA_BIT:
    case ELIB_SIMBUS_SB_TX_PARITY_BIT:
    case ELIB_SIMBUS_SB_TX_STOP_BIT:
        return 1;
    default:
        return 0;
    }
}
