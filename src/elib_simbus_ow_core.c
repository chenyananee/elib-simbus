/* elib_simbus_ow_core.c - SimBus 1-Wire Implementation */

#include "elib_simbus_ow_core.h"
#include <stddef.h>

/*
 * Timing multipliers (× delay_num):
 *   Reset low     : 480
 *   Reset sample  : 70  (release then wait before sampling)
 *   Write-1 low   : 6
 *   Write-1 slot  : 64
 *   Write-0 low   : 60
 *   Read low      : 6
 *   Read sample   : 9   (delay after release before read)
 *   Read slot     : 60
 */

#define OW_RESET_LOW      480
#define OW_RESET_SAMPLE    70
#define OW_WRITE1_LOW       6
#define OW_WRITE1_SLOT     64
#define OW_WRITE0_LOW      60
#define OW_READ_LOW         6
#define OW_READ_SAMPLE      9
#define OW_READ_SLOT       60

/* ------------------------------------------------------------------ */
/*  Internal helpers                                                    */
/* ------------------------------------------------------------------ */

static inline void ow_line_low(elib_simbus_ow_ctx_t *ctx)
{
    ctx->io_write(ctx->dq_pin, 0);
    ctx->io_setdir(ctx->dq_pin, 1);
}

static inline void ow_line_release(elib_simbus_ow_ctx_t *ctx)
{
    ctx->io_setdir(ctx->dq_pin, 0);
}

static inline uint8_t ow_line_read(elib_simbus_ow_ctx_t *ctx)
{
    return ctx->io_read(ctx->dq_pin);
}

static inline void ow_delay(elib_simbus_ow_ctx_t *ctx, uint32_t mult)
{
    ctx->delay_us(ctx->delay_num * mult);
}

/* ------------------------------------------------------------------ */
/*  Public API                                                          */
/* ------------------------------------------------------------------ */

elib_simbus_err_t elib_simbus_ow_init(
    elib_simbus_ow_ctx_t *ctx,
    const elib_simbus_ow_cfg_t *cfg)
{
    if (ctx == NULL || cfg == NULL ||
        cfg->io_write == NULL || cfg->io_read == NULL ||
        cfg->io_setdir == NULL || cfg->delay_us == NULL) {
        return ELIB_SIMBUS_ERR_INVALID_PARAM;
    }
    if (cfg->delay_num == 0) {
        return ELIB_SIMBUS_ERR_INVALID_PARAM;
    }

    ctx->dq_pin         = cfg->dq_pin;
    ctx->delay_num      = cfg->delay_num;
    ctx->timeout_rounds = (cfg->timeout_rounds == 0) ?
                           ELIB_SIMBUS_OW_DEFAULT_TIMEOUT : cfg->timeout_rounds;
    ctx->io_write       = cfg->io_write;
    ctx->io_read        = cfg->io_read;
    ctx->io_setdir      = cfg->io_setdir;
    ctx->delay_us       = cfg->delay_us;
    ctx->bit_flags.initialized = 1;
    ctx->bit_flags.timeout     = 0;

    return ELIB_SIMBUS_OK;
}

void elib_simbus_ow_deinit(elib_simbus_ow_ctx_t *ctx)
{
    if (ctx == NULL) return;
    ctx->bit_flags.initialized = 0;
}

int elib_simbus_ow_reset(elib_simbus_ow_ctx_t *ctx)
{
    if (ctx == NULL || !ctx->bit_flags.initialized) return -1;

    ctx->bit_flags.timeout = 0;

    ow_line_low(ctx);
    ow_delay(ctx, OW_RESET_LOW);

    ow_line_release(ctx);
    ow_delay(ctx, OW_RESET_SAMPLE);

    /* Poll for presence pulse (slave pulls low within 60µs) */
    ctx->wait_rounds = 0;
    while (ow_line_read(ctx)) {
        if (ctx->wait_rounds >= ctx->timeout_rounds) {
            ctx->bit_flags.timeout = 1;
            return -1;
        }
        ow_delay(ctx, 1);
        ctx->wait_rounds++;
    }

    /* Wait rest of presence pulse, then verify release */
    ow_delay(ctx, OW_RESET_LOW - OW_RESET_SAMPLE);

    return 1;
}

void elib_simbus_ow_write_bit(elib_simbus_ow_ctx_t *ctx, uint8_t bit)
{
    if (bit) {
        /* Write-1: short low, release, wait rest of slot */
        ow_line_low(ctx);
        ow_delay(ctx, OW_WRITE1_LOW);
        ow_line_release(ctx);
        ow_delay(ctx, OW_WRITE1_SLOT - OW_WRITE1_LOW);
    } else {
        /* Write-0: long low, release */
        ow_line_low(ctx);
        ow_delay(ctx, OW_WRITE0_LOW);
        ow_line_release(ctx);
        ow_delay(ctx, 1);  /* recovery */
    }
}

uint8_t elib_simbus_ow_read_bit(elib_simbus_ow_ctx_t *ctx)
{
    uint8_t bit;

    ow_line_low(ctx);
    ow_delay(ctx, OW_READ_LOW);
    ow_line_release(ctx);
    ow_delay(ctx, OW_READ_SAMPLE);

    bit = ow_line_read(ctx);

    ow_delay(ctx, OW_READ_SLOT - OW_READ_LOW - OW_READ_SAMPLE);

    return bit & 1;
}

void elib_simbus_ow_write_byte(elib_simbus_ow_ctx_t *ctx, uint8_t byte)
{
    for (uint32_t i = 0; i < 8; i++) {
        elib_simbus_ow_write_bit(ctx, (byte >> i) & 1);
    }
}

uint8_t elib_simbus_ow_read_byte(elib_simbus_ow_ctx_t *ctx)
{
    uint8_t byte = 0;

    for (uint32_t i = 0; i < 8; i++) {
        if (elib_simbus_ow_read_bit(ctx)) {
            byte |= (uint8_t)(1U << i);
        }
    }

    return byte;
}

elib_simbus_err_t elib_simbus_ow_write(
    elib_simbus_ow_ctx_t *ctx,
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
        elib_simbus_ow_write_byte(ctx, buf[i]);
    }

    return ELIB_SIMBUS_OK;
}

elib_simbus_err_t elib_simbus_ow_read(
    elib_simbus_ow_ctx_t *ctx,
    void *data,
    uint32_t len,
    uint32_t max_len)
{
    if (ctx == NULL || data == NULL) return ELIB_SIMBUS_ERR_INVALID_PARAM;
    if (!ctx->bit_flags.initialized) return ELIB_SIMBUS_ERR_NOT_INITIALIZED;
    if (len > max_len) return ELIB_SIMBUS_ERR_EXCEED_MAX;
    if (len == 0) return ELIB_SIMBUS_OK;

    uint8_t *buf = (uint8_t *)data;
    for (uint32_t i = 0; i < len; i++) {
        buf[i] = elib_simbus_ow_read_byte(ctx);
    }

    return ELIB_SIMBUS_OK;
}
