/* elib_simbus_i2c_core.c - SimBus I2C Bit-Bang Implementation */

#include "elib_simbus_i2c_core.h"
#include <stddef.h>

#define ELIB_SIMBUS_I2C_DEFAULT_MAX_WAIT 100

/* ------------------------------------------------------------------ */
/*  Internal helpers                                                    */
/* ------------------------------------------------------------------ */

static inline void i2c_delay(elib_simbus_i2c_ctx_t *ctx)
{
    ctx->delay_us(ctx->delay_num);
}

static inline void i2c_scl_low(elib_simbus_i2c_ctx_t *ctx)
{
    ctx->io_write(ctx->scl_pin, 0);
    ctx->io_setdir(ctx->scl_pin, 1);
}

static inline void i2c_scl_high(elib_simbus_i2c_ctx_t *ctx)
{
    ctx->io_write(ctx->scl_pin, 1);
    ctx->io_setdir(ctx->scl_pin, 1);
}

static inline void i2c_sda_low(elib_simbus_i2c_ctx_t *ctx)
{
    ctx->io_write(ctx->sda_pin, 0);
    ctx->io_setdir(ctx->sda_pin, 1);
}

static inline void i2c_sda_release(elib_simbus_i2c_ctx_t *ctx)
{
    ctx->io_setdir(ctx->sda_pin, 0);
}

static inline uint8_t i2c_sda_read(elib_simbus_i2c_ctx_t *ctx)
{
    return ctx->io_read(ctx->sda_pin);
}

/* ------------------------------------------------------------------ */
/*  I2C protocol primitives                                             */
/* ------------------------------------------------------------------ */

static void i2c_start(elib_simbus_i2c_ctx_t *ctx)
{
    i2c_sda_release(ctx);
    i2c_delay(ctx);
    i2c_scl_high(ctx);
    i2c_delay(ctx);
    i2c_sda_low(ctx);
    i2c_delay(ctx);
    i2c_scl_low(ctx);
    i2c_delay(ctx);
}

static void i2c_stop(elib_simbus_i2c_ctx_t *ctx)
{
    i2c_sda_low(ctx);
    i2c_delay(ctx);
    i2c_scl_high(ctx);
    i2c_delay(ctx);
    i2c_sda_release(ctx);
    i2c_delay(ctx);
}

static uint8_t i2c_write_byte(elib_simbus_i2c_ctx_t *ctx, uint8_t byte)
{
    for (int32_t i = 7; i >= 0; i--) {
        if (byte & (1U << i)) {
            i2c_sda_release(ctx);
        } else {
            i2c_sda_low(ctx);
        }
        i2c_delay(ctx);
        i2c_scl_high(ctx);
        i2c_delay(ctx);
        i2c_scl_low(ctx);
        i2c_delay(ctx);
    }

    /* ACK phase: release SDA, raise SCL, poll SDA with timeout */
    i2c_sda_release(ctx);
    i2c_delay(ctx);
    i2c_scl_high(ctx);
    i2c_delay(ctx);

    ctx->wait_rounds = 0;
    uint8_t ack = i2c_sda_read(ctx);
    while (ack != 0) {
        if (ctx->wait_rounds >= ctx->max_wait) {
            ctx->bit_flags.timeout = 1;
            break;
        }
        ctx->delay_us(ctx->delay_num);
        ctx->wait_rounds++;
        ack = i2c_sda_read(ctx);
    }

    i2c_scl_low(ctx);
    return ack;
}

static uint8_t i2c_read_byte(elib_simbus_i2c_ctx_t *ctx, uint8_t ack)
{
    uint8_t byte = 0;

    i2c_sda_release(ctx);
    for (int32_t i = 7; i >= 0; i--) {
        i2c_scl_high(ctx);
        i2c_delay(ctx);
        byte = (byte << 1) | i2c_sda_read(ctx);
        i2c_scl_low(ctx);
        i2c_delay(ctx);
    }

    if (ack) {
        i2c_sda_release(ctx);
    } else {
        i2c_sda_low(ctx);
    }
    i2c_delay(ctx);
    i2c_scl_high(ctx);
    i2c_delay(ctx);
    i2c_scl_low(ctx);
    i2c_sda_release(ctx);

    return byte;
}

/* ------------------------------------------------------------------ */
/*  Public API                                                          */
/* ------------------------------------------------------------------ */

elib_simbus_err_t elib_simbus_i2c_init(
    elib_simbus_i2c_ctx_t *ctx,
    const elib_simbus_i2c_cfg_t *cfg)
{
    if (ctx == NULL || cfg == NULL ||
        cfg->io_write == NULL || cfg->io_read == NULL ||
        cfg->io_setdir == NULL || cfg->delay_us == NULL) {
        return ELIB_SIMBUS_ERR_INVALID_PARAM;
    }

    ctx->scl_pin   = cfg->scl_pin;
    ctx->sda_pin   = cfg->sda_pin;
    ctx->delay_num = cfg->delay_num;
    ctx->max_wait  = (cfg->max_wait == 0) ? ELIB_SIMBUS_I2C_DEFAULT_MAX_WAIT : cfg->max_wait;
    ctx->io_write  = cfg->io_write;
    ctx->io_read   = cfg->io_read;
    ctx->io_setdir = cfg->io_setdir;
    ctx->delay_us  = cfg->delay_us;
    ctx->bit_flags.initialized = 1;
    ctx->bit_flags.timeout     = 0;

    return ELIB_SIMBUS_OK;
}

void elib_simbus_i2c_deinit(elib_simbus_i2c_ctx_t *ctx)
{
    if (ctx == NULL) {
        return;
    }
    ctx->bit_flags.initialized = 0;
}

elib_simbus_err_t elib_simbus_i2c_write(
    elib_simbus_i2c_ctx_t *ctx,
    uint8_t dev_addr,
    const void *data,
    uint32_t len,
    uint32_t max_len)
{
    if (ctx == NULL || data == NULL) {
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

    ctx->bit_flags.timeout = 0;

    const uint8_t *buf = (const uint8_t *)data;

    i2c_start(ctx);
    if (i2c_write_byte(ctx, (uint8_t)(dev_addr << 1))) {
        i2c_stop(ctx);
        if (ctx->bit_flags.timeout) return ELIB_SIMBUS_ERR_TIMEOUT;
        return ELIB_SIMBUS_ERR_NACK;
    }

    for (uint32_t i = 0; i < len; i++) {
        if (i2c_write_byte(ctx, buf[i])) {
            i2c_stop(ctx);
            if (ctx->bit_flags.timeout) return ELIB_SIMBUS_ERR_TIMEOUT;
            return ELIB_SIMBUS_ERR_NACK;
        }
    }

    i2c_stop(ctx);
    return ELIB_SIMBUS_OK;
}

elib_simbus_err_t elib_simbus_i2c_read(
    elib_simbus_i2c_ctx_t *ctx,
    uint8_t dev_addr,
    void *data,
    uint32_t len,
    uint32_t max_len)
{
    if (ctx == NULL || data == NULL) {
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

    ctx->bit_flags.timeout = 0;

    uint8_t *buf = (uint8_t *)data;

    i2c_start(ctx);
    if (i2c_write_byte(ctx, (uint8_t)((dev_addr << 1) | 1))) {
        i2c_stop(ctx);
        if (ctx->bit_flags.timeout) return ELIB_SIMBUS_ERR_TIMEOUT;
        return ELIB_SIMBUS_ERR_NACK;
    }

    for (uint32_t i = 0; i < len; i++) {
        buf[i] = i2c_read_byte(ctx, (uint8_t)((i == len - 1) ? 1 : 0));
    }

    i2c_stop(ctx);
    return ELIB_SIMBUS_OK;
}

elib_simbus_err_t elib_simbus_i2c_write_mem(
    elib_simbus_i2c_ctx_t *ctx,
    uint8_t dev_addr,
    uint32_t mem_addr,
    uint32_t mem_addr_len,
    const void *data,
    uint32_t len,
    uint32_t max_len)
{
    if (ctx == NULL || data == NULL || mem_addr_len == 0 || mem_addr_len > 4) {
        return ELIB_SIMBUS_ERR_INVALID_PARAM;
    }
    if (!ctx->bit_flags.initialized) {
        return ELIB_SIMBUS_ERR_NOT_INITIALIZED;
    }
    if (len > max_len) {
        return ELIB_SIMBUS_ERR_EXCEED_MAX;
    }

    ctx->bit_flags.timeout = 0;

    const uint8_t *buf = (const uint8_t *)data;

    i2c_start(ctx);
    if (i2c_write_byte(ctx, (uint8_t)(dev_addr << 1))) {
        i2c_stop(ctx);
        if (ctx->bit_flags.timeout) return ELIB_SIMBUS_ERR_TIMEOUT;
        return ELIB_SIMBUS_ERR_NACK;
    }

    for (int32_t i = (int32_t)mem_addr_len - 1; i >= 0; i--) {
        uint8_t addr_byte = (uint8_t)((mem_addr >> ((uint32_t)i * 8)) & 0xFF);
        if (i2c_write_byte(ctx, addr_byte)) {
            i2c_stop(ctx);
            if (ctx->bit_flags.timeout) return ELIB_SIMBUS_ERR_TIMEOUT;
            return ELIB_SIMBUS_ERR_NACK;
        }
    }

    for (uint32_t i = 0; i < len; i++) {
        if (i2c_write_byte(ctx, buf[i])) {
            i2c_stop(ctx);
            if (ctx->bit_flags.timeout) return ELIB_SIMBUS_ERR_TIMEOUT;
            return ELIB_SIMBUS_ERR_NACK;
        }
    }

    i2c_stop(ctx);
    return ELIB_SIMBUS_OK;
}

elib_simbus_err_t elib_simbus_i2c_read_mem(
    elib_simbus_i2c_ctx_t *ctx,
    uint8_t dev_addr,
    uint32_t mem_addr,
    uint32_t mem_addr_len,
    void *data,
    uint32_t len,
    uint32_t max_len)
{
    if (ctx == NULL || data == NULL || mem_addr_len == 0 || mem_addr_len > 4) {
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

    ctx->bit_flags.timeout = 0;

    uint8_t *buf = (uint8_t *)data;

    i2c_start(ctx);
    if (i2c_write_byte(ctx, (uint8_t)(dev_addr << 1))) {
        i2c_stop(ctx);
        if (ctx->bit_flags.timeout) return ELIB_SIMBUS_ERR_TIMEOUT;
        return ELIB_SIMBUS_ERR_NACK;
    }

    for (int32_t i = (int32_t)mem_addr_len - 1; i >= 0; i--) {
        uint8_t addr_byte = (uint8_t)((mem_addr >> ((uint32_t)i * 8)) & 0xFF);
        if (i2c_write_byte(ctx, addr_byte)) {
            i2c_stop(ctx);
            if (ctx->bit_flags.timeout) return ELIB_SIMBUS_ERR_TIMEOUT;
            return ELIB_SIMBUS_ERR_NACK;
        }
    }

    i2c_start(ctx);

    if (i2c_write_byte(ctx, (uint8_t)((dev_addr << 1) | 1))) {
        i2c_stop(ctx);
        if (ctx->bit_flags.timeout) return ELIB_SIMBUS_ERR_TIMEOUT;
        return ELIB_SIMBUS_ERR_NACK;
    }

    for (uint32_t i = 0; i < len; i++) {
        buf[i] = i2c_read_byte(ctx, (uint8_t)((i == len - 1) ? 1 : 0));
    }

    i2c_stop(ctx);
    return ELIB_SIMBUS_OK;
}
