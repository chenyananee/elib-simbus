/* elib_simbus_ws2812_core.c - SimBus WS2812 Implementation */

#include "elib_simbus_ws2812_core.h"
#include <stddef.h>

#define ELIB_SIMBUS_WS2812_DEFAULT_T0H    350
#define ELIB_SIMBUS_WS2812_DEFAULT_T0L    800
#define ELIB_SIMBUS_WS2812_DEFAULT_T1H    700
#define ELIB_SIMBUS_WS2812_DEFAULT_T1L    600
#define ELIB_SIMBUS_WS2812_DEFAULT_RESET 55000

/* ------------------------------------------------------------------ */
/*  Internal helpers                                                    */
/* ------------------------------------------------------------------ */

static inline void pin_low(elib_simbus_ws2812_ctx_t *ctx)
{
    ctx->io_write(ctx->dq_pin, 0);
    ctx->io_setdir(ctx->dq_pin, 1);
}

static inline void pin_high(elib_simbus_ws2812_ctx_t *ctx)
{
    ctx->io_write(ctx->dq_pin, 1);
    ctx->io_setdir(ctx->dq_pin, 1);
}

static inline void pin_release(elib_simbus_ws2812_ctx_t *ctx)
{
    ctx->io_setdir(ctx->dq_pin, 0);
}

static void send_bit(elib_simbus_ws2812_ctx_t *ctx, uint8_t bit)
{
    if (bit) {
        pin_high(ctx);
        ctx->delay_ns(ctx->t1h);
        pin_low(ctx);
        ctx->delay_ns(ctx->t1l);
    } else {
        pin_high(ctx);
        ctx->delay_ns(ctx->t0h);
        pin_low(ctx);
        ctx->delay_ns(ctx->t0l);
    }
}

/* ------------------------------------------------------------------ */
/*  Public API                                                          */
/* ------------------------------------------------------------------ */

elib_simbus_err_t elib_simbus_ws2812_init(
    elib_simbus_ws2812_ctx_t *ctx,
    const elib_simbus_ws2812_cfg_t *cfg)
{
    if (ctx == NULL || cfg == NULL ||
        cfg->io_write == NULL || cfg->io_read == NULL ||
        cfg->io_setdir == NULL || cfg->delay_ns == NULL) {
        return ELIB_SIMBUS_ERR_INVALID_PARAM;
    }

    ctx->dq_pin   = cfg->dq_pin;
    ctx->t0h      = (cfg->t0h == 0) ? ELIB_SIMBUS_WS2812_DEFAULT_T0H : cfg->t0h;
    ctx->t0l      = (cfg->t0l == 0) ? ELIB_SIMBUS_WS2812_DEFAULT_T0L : cfg->t0l;
    ctx->t1h      = (cfg->t1h == 0) ? ELIB_SIMBUS_WS2812_DEFAULT_T1H : cfg->t1h;
    ctx->t1l      = (cfg->t1l == 0) ? ELIB_SIMBUS_WS2812_DEFAULT_T1L : cfg->t1l;
    ctx->reset_ns = (cfg->reset_ns == 0) ? ELIB_SIMBUS_WS2812_DEFAULT_RESET : cfg->reset_ns;
    ctx->io_write = cfg->io_write;
    ctx->io_read  = cfg->io_read;
    ctx->io_setdir = cfg->io_setdir;
    ctx->delay_ns = cfg->delay_ns;
    ctx->bit_flags.initialized = 1;

    return ELIB_SIMBUS_OK;
}

void elib_simbus_ws2812_deinit(elib_simbus_ws2812_ctx_t *ctx)
{
    if (ctx == NULL) return;
    ctx->bit_flags.initialized = 0;
}

elib_simbus_err_t elib_simbus_ws2812_send(
    elib_simbus_ws2812_ctx_t *ctx,
    const void *data,
    uint32_t len)
{
    if (ctx == NULL || data == NULL) return ELIB_SIMBUS_ERR_INVALID_PARAM;
    if (!ctx->bit_flags.initialized) return ELIB_SIMBUS_ERR_NOT_INITIALIZED;
    if (len == 0) return ELIB_SIMBUS_OK;

    const uint8_t *buf = (const uint8_t *)data;

    for (uint32_t i = 0; i < len; i++) {
        for (int32_t bit = 7; bit >= 0; bit--) {
            send_bit(ctx, (buf[i] >> bit) & 1);
        }
    }

    /* Reset: pull low, hold, release */
    pin_low(ctx);
    ctx->delay_ns(ctx->reset_ns);
    pin_release(ctx);

    return ELIB_SIMBUS_OK;
}

elib_simbus_err_t elib_simbus_ws2812_send_rgb(
    elib_simbus_ws2812_ctx_t *ctx,
    const uint8_t *rgb,
    uint32_t count)
{
    if (ctx == NULL || rgb == NULL) return ELIB_SIMBUS_ERR_INVALID_PARAM;
    if (!ctx->bit_flags.initialized) return ELIB_SIMBUS_ERR_NOT_INITIALIZED;
    if (count == 0) return ELIB_SIMBUS_OK;

    /* WS2812 expects GRB order, so convert RGB → GRB */
    for (uint32_t i = 0; i < count; i++) {
        uint8_t r = rgb[i * 3 + 0];
        uint8_t g = rgb[i * 3 + 1];
        uint8_t b = rgb[i * 3 + 2];

        /* Send G byte first */
        for (int32_t bit = 7; bit >= 0; bit--) {
            send_bit(ctx, (g >> bit) & 1);
        }
        /* Send R byte */
        for (int32_t bit = 7; bit >= 0; bit--) {
            send_bit(ctx, (r >> bit) & 1);
        }
        /* Send B byte */
        for (int32_t bit = 7; bit >= 0; bit--) {
            send_bit(ctx, (b >> bit) & 1);
        }
    }

    /* Reset */
    pin_low(ctx);
    ctx->delay_ns(ctx->reset_ns);
    pin_release(ctx);

    return ELIB_SIMBUS_OK;
}
