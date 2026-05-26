/* elib_simbus_ow.h - SimBus 1-Wire Master */

#ifndef ELIB_SIMBUS_OW_H
#define ELIB_SIMBUS_OW_H

#include <stdint.h>
#include "elib_simbus_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ------------------------------------------------------------------ */
/*  Callback type definitions                                          */
/* ------------------------------------------------------------------ */

typedef void (*elib_simbus_ow_io_write_t)(uint8_t pin, uint8_t level);
typedef uint8_t (*elib_simbus_ow_io_read_t)(uint8_t pin);
typedef void (*elib_simbus_ow_io_setdir_t)(uint8_t pin, uint8_t dir);
typedef void (*elib_simbus_ow_delay_us_t)(uint32_t us);

/* ------------------------------------------------------------------ */
/*  Configuration                                                      */
/* ------------------------------------------------------------------ */

#define ELIB_SIMBUS_OW_DEFAULT_TIMEOUT 100

typedef struct {
    uint8_t  dq_pin;
    uint32_t delay_num;          /* 时基 (µs), 标准 = 1 */
    uint32_t timeout_rounds;     /* 存在脉冲检测超时轮数 */

    elib_simbus_ow_io_write_t  io_write;
    elib_simbus_ow_io_read_t   io_read;
    elib_simbus_ow_io_setdir_t io_setdir;
    elib_simbus_ow_delay_us_t  delay_us;
} elib_simbus_ow_cfg_t;

/* ------------------------------------------------------------------ */
/*  Context                                                            */
/* ------------------------------------------------------------------ */

typedef struct {
    uint8_t  dq_pin;
    uint32_t delay_num;
    uint32_t timeout_rounds;
    uint32_t wait_rounds;

    elib_simbus_ow_io_write_t  io_write;
    elib_simbus_ow_io_read_t   io_read;
    elib_simbus_ow_io_setdir_t io_setdir;
    elib_simbus_ow_delay_us_t  delay_us;

    struct {
        uint8_t initialized : 1;
        uint8_t timeout     : 1;
        uint8_t reserved    : 6;
    } bit_flags;
} elib_simbus_ow_ctx_t;

/* ------------------------------------------------------------------ */
/*  API                                                                */
/* ------------------------------------------------------------------ */

elib_simbus_err_t elib_simbus_ow_init(
    elib_simbus_ow_ctx_t *ctx,
    const elib_simbus_ow_cfg_t *cfg);

void elib_simbus_ow_deinit(elib_simbus_ow_ctx_t *ctx);

/**
 * @brief Bus reset and presence detect
 * @return 1 = device present, 0 = no device, -1 = timeout
 */
int elib_simbus_ow_reset(elib_simbus_ow_ctx_t *ctx);

/**
 * @brief Write one bit
 */
void elib_simbus_ow_write_bit(elib_simbus_ow_ctx_t *ctx, uint8_t bit);

/**
 * @brief Read one bit
 * @return 0 or 1
 */
uint8_t elib_simbus_ow_read_bit(elib_simbus_ow_ctx_t *ctx);

/**
 * @brief Write one byte (LSB first)
 */
void elib_simbus_ow_write_byte(elib_simbus_ow_ctx_t *ctx, uint8_t byte);

/**
 * @brief Read one byte (LSB first)
 * @return 0-255
 */
uint8_t elib_simbus_ow_read_byte(elib_simbus_ow_ctx_t *ctx);

/**
 * @brief Write multiple bytes
 */
elib_simbus_err_t elib_simbus_ow_write(
    elib_simbus_ow_ctx_t *ctx,
    const void *data,
    uint32_t len,
    uint32_t max_len);

/**
 * @brief Read multiple bytes
 */
elib_simbus_err_t elib_simbus_ow_read(
    elib_simbus_ow_ctx_t *ctx,
    void *data,
    uint32_t len,
    uint32_t max_len);

#ifdef __cplusplus
}
#endif

#endif /* ELIB_SIMBUS_OW_H */
