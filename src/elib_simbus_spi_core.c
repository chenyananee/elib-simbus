/* elib_simbus_spi_core.c - SimBus SPI Bit-Bang Implementation */

#include "elib_simbus_spi_core.h"
#include <stddef.h>

/* ------------------------------------------------------------------ */
/*  Internal helpers                                                    */
/* ------------------------------------------------------------------ */

static inline void spi_delay(elib_simbus_spi_ctx_t *ctx)
{
    ctx->delay_us(ctx->delay_num);
}

static inline void spi_sclk_low(elib_simbus_spi_ctx_t *ctx)
{
    ctx->io_write(ctx->sclk_pin, 0);
    ctx->io_setdir(ctx->sclk_pin, 1);
}

static inline void spi_sclk_high(elib_simbus_spi_ctx_t *ctx)
{
    ctx->io_write(ctx->sclk_pin, 1);
    ctx->io_setdir(ctx->sclk_pin, 1);
}

static inline void spi_mosi_set(elib_simbus_spi_ctx_t *ctx, uint8_t level)
{
    ctx->io_write(ctx->mosi_pin, level);
    ctx->io_setdir(ctx->mosi_pin, 1);
}

static inline uint8_t spi_miso_read(elib_simbus_spi_ctx_t *ctx)
{
    return ctx->io_read(ctx->miso_pin);
}

/* ------------------------------------------------------------------ */
/*  SPI byte transfer (all 4 modes)                                     */
/* ------------------------------------------------------------------ */

static uint8_t spi_xfer_byte(elib_simbus_spi_ctx_t *ctx, uint8_t tx_byte)
{
    uint8_t rx_byte = 0;
    int32_t i, end, step;

    if (ctx->bit_order) {
        i = 0; end = 7; step = 1;
    } else {
        i = 7; end = 0; step = -1;
    }

    switch (ctx->mode) {
    case 0: /* CPOL=0, CPHA=0: idle low, capture on rising */
        for (;;) {
            spi_mosi_set(ctx, (tx_byte >> i) & 1);
            spi_delay(ctx);
            spi_sclk_high(ctx);
            spi_delay(ctx);
            if (spi_miso_read(ctx)) rx_byte |= (1U << i);
            spi_sclk_low(ctx);
            if (i == end) break;
            i += step;
        }
        break;

    case 1: /* CPOL=0, CPHA=1: idle low, capture on falling */
        for (;;) {
            spi_mosi_set(ctx, (tx_byte >> i) & 1);
            spi_sclk_high(ctx);
            spi_delay(ctx);
            spi_sclk_low(ctx);
            spi_delay(ctx);
            if (spi_miso_read(ctx)) rx_byte |= (1U << i);
            if (i == end) break;
            i += step;
        }
        break;

    case 2: /* CPOL=1, CPHA=0: idle high, capture on falling */
        for (;;) {
            spi_mosi_set(ctx, (tx_byte >> i) & 1);
            spi_delay(ctx);
            spi_sclk_low(ctx);
            spi_delay(ctx);
            if (spi_miso_read(ctx)) rx_byte |= (1U << i);
            spi_sclk_high(ctx);
            if (i == end) break;
            i += step;
        }
        break;

    case 3: /* CPOL=1, CPHA=1: idle high, capture on rising */
        for (;;) {
            spi_mosi_set(ctx, (tx_byte >> i) & 1);
            spi_sclk_low(ctx);
            spi_delay(ctx);
            spi_sclk_high(ctx);
            spi_delay(ctx);
            if (spi_miso_read(ctx)) rx_byte |= (1U << i);
            if (i == end) break;
            i += step;
        }
        break;
    }

    return rx_byte;
}

/* ------------------------------------------------------------------ */
/*  Public API                                                          */
/* ------------------------------------------------------------------ */

elib_simbus_err_t elib_simbus_spi_init(
    elib_simbus_spi_ctx_t *ctx,
    const elib_simbus_spi_cfg_t *cfg)
{
    if (ctx == NULL || cfg == NULL ||
        cfg->io_write == NULL || cfg->io_read == NULL ||
        cfg->io_setdir == NULL || cfg->delay_us == NULL) {
        return ELIB_SIMBUS_ERR_INVALID_PARAM;
    }
    if (cfg->mode > 3) {
        return ELIB_SIMBUS_ERR_INVALID_PARAM;
    }

    ctx->sclk_pin   = cfg->sclk_pin;
    ctx->mosi_pin   = cfg->mosi_pin;
    ctx->miso_pin   = cfg->miso_pin;
    ctx->cs_pin     = cfg->cs_pin;
    ctx->delay_num  = cfg->delay_num;
    ctx->mode       = cfg->mode;
    ctx->bit_order  = cfg->bit_order;
    ctx->dummy_byte = cfg->dummy_byte;
    ctx->io_write   = cfg->io_write;
    ctx->io_read    = cfg->io_read;
    ctx->io_setdir  = cfg->io_setdir;
    ctx->delay_us   = cfg->delay_us;
    ctx->bit_flags.initialized = 1;

    return ELIB_SIMBUS_OK;
}

void elib_simbus_spi_deinit(elib_simbus_spi_ctx_t *ctx)
{
    if (ctx == NULL) return;
    ctx->bit_flags.initialized = 0;
}

elib_simbus_err_t elib_simbus_spi_transfer(
    elib_simbus_spi_ctx_t *ctx,
    const void *tx_data,
    void *rx_data,
    uint32_t len,
    uint32_t max_len)
{
    if (ctx == NULL) {
        return ELIB_SIMBUS_ERR_INVALID_PARAM;
    }
    if (!ctx->bit_flags.initialized) {
        return ELIB_SIMBUS_ERR_NOT_INITIALIZED;
    }
    if (len > max_len) {
        return ELIB_SIMBUS_ERR_EXCEED_MAX;
    }
    if (len == 0) {
        return ELIB_SIMBUS_OK;
    }

    /* Set MISO as input */
    ctx->io_setdir(ctx->miso_pin, 0);

    const uint8_t *tx_buf = (const uint8_t *)tx_data;
    uint8_t *rx_buf = (uint8_t *)rx_data;

    for (uint32_t i = 0; i < len; i++) {
        uint8_t tx_byte = tx_buf ? tx_buf[i] : ctx->dummy_byte;
        uint8_t rx_byte = spi_xfer_byte(ctx, tx_byte);
        if (rx_buf) rx_buf[i] = rx_byte;
    }

    return ELIB_SIMBUS_OK;
}

elib_simbus_err_t elib_simbus_spi_write(
    elib_simbus_spi_ctx_t *ctx,
    const void *data,
    uint32_t len,
    uint32_t max_len)
{
    return elib_simbus_spi_transfer(ctx, data, NULL, len, max_len);
}

elib_simbus_err_t elib_simbus_spi_read(
    elib_simbus_spi_ctx_t *ctx,
    void *data,
    uint32_t len,
    uint32_t max_len)
{
    return elib_simbus_spi_transfer(ctx, NULL, data, len, max_len);
}

void elib_simbus_spi_cs_low(elib_simbus_spi_ctx_t *ctx)
{
    if (ctx == NULL || !ctx->bit_flags.initialized) return;
    ctx->io_write(ctx->cs_pin, 0);
    ctx->io_setdir(ctx->cs_pin, 1);
}

void elib_simbus_spi_cs_high(elib_simbus_spi_ctx_t *ctx)
{
    if (ctx == NULL || !ctx->bit_flags.initialized) return;
    ctx->io_write(ctx->cs_pin, 1);
    ctx->io_setdir(ctx->cs_pin, 1);
}
