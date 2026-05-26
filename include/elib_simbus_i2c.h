/* elib_simbus_i2c.h - SimBus I2C Master (Bit-Bang) */

#ifndef ELIB_SIMBUS_I2C_H
#define ELIB_SIMBUS_I2C_H

#include <stdint.h>
#include "elib_simbus_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ------------------------------------------------------------------ */
/*  Callback type definitions                                          */
/* ------------------------------------------------------------------ */

/**
 * @brief Write a GPIO pin level (0 or 1)
 * @param pin  Pin identifier
 * @param level  0 = low, 1 = high
 */
typedef void (*elib_simbus_i2c_io_write_t)(uint8_t pin, uint8_t level);

/**
 * @brief Read a GPIO pin level
 * @param pin  Pin identifier
 * @return 0 = low, 1 = high
 */
typedef uint8_t (*elib_simbus_i2c_io_read_t)(uint8_t pin);

/**
 * @brief Set GPIO pin direction
 * @param pin Pin identifier
 * @param dir  0 = input (high-Z), 1 = output
 */
typedef void (*elib_simbus_i2c_io_setdir_t)(uint8_t pin, uint8_t dir);

/**
 * @brief Microsecond delay
 * @param us Delay time in microseconds
 */
typedef void (*elib_simbus_i2c_delay_us_t)(uint32_t us);

/* ------------------------------------------------------------------ */
/*  Configuration (zero-temporary for init)                             */
/* ------------------------------------------------------------------ */

typedef struct {
    uint8_t  scl_pin;
    uint8_t  sda_pin;
    uint32_t delay_num;
    uint32_t max_wait;

    elib_simbus_i2c_io_write_t  io_write;
    elib_simbus_i2c_io_read_t   io_read;
    elib_simbus_i2c_io_setdir_t io_setdir;
    elib_simbus_i2c_delay_us_t  delay_us;
} elib_simbus_i2c_cfg_t;

/* ------------------------------------------------------------------ */
/*  Context                                                            */
/* ------------------------------------------------------------------ */

typedef struct {
    uint8_t  scl_pin;
    uint8_t  sda_pin;
    uint32_t delay_num;
    uint32_t max_wait;
    uint32_t wait_rounds;

    elib_simbus_i2c_io_write_t  io_write;
    elib_simbus_i2c_io_read_t   io_read;
    elib_simbus_i2c_io_setdir_t io_setdir;
    elib_simbus_i2c_delay_us_t  delay_us;

    struct {
        uint8_t initialized : 1;
        uint8_t timeout     : 1;
        uint8_t reserved    : 6;
    } bit_flags;
} elib_simbus_i2c_ctx_t;

/* ------------------------------------------------------------------ */
/*  API                                                                */
/* ------------------------------------------------------------------ */

/**
 * @brief Initialize I2C master context from config
 * @param ctx User-allocated context
 * @param cfg Configuration (can be a compound literal)
 * @return ELIB_SIMBUS_OK on success
 */
elib_simbus_err_t elib_simbus_i2c_init(
    elib_simbus_i2c_ctx_t *ctx,
    const elib_simbus_i2c_cfg_t *cfg);

/**
 * @brief Deinitialize I2C master context
 * @param ctx I2C context
 */
void elib_simbus_i2c_deinit(elib_simbus_i2c_ctx_t *ctx);

/**
 * @brief Write data to I2C slave
 * @param ctx I2C context
 * @param dev_addr 7-bit slave address
 * @param data Data to write
 * @param len Number of bytes to write
 * @param max_len Maximum allowed bytes to write
 * @return ELIB_SIMBUS_OK on success
 */
elib_simbus_err_t elib_simbus_i2c_write(
    elib_simbus_i2c_ctx_t *ctx,
    uint8_t dev_addr,
    const void *data,
    uint32_t len,
    uint32_t max_len);

/**
 * @brief Read data from I2C slave
 * @param ctx I2C context
 * @param dev_addr 7-bit slave address
 * @param data Buffer to store read data
 * @param len Number of bytes to read
 * @param max_len Maximum allowed bytes to read
 * @return ELIB_SIMBUS_OK on success
 */
elib_simbus_err_t elib_simbus_i2c_read(
    elib_simbus_i2c_ctx_t *ctx,
    uint8_t dev_addr,
    void *data,
    uint32_t len,
    uint32_t max_len);

/**
 * @brief Write to I2C slave memory/register
 * @param ctx I2C context
 * @param dev_addr 7-bit slave address
 * @param mem_addr Memory/register address
 * @param mem_addr_len Number of address bytes (1-4)
 * @param data Data to write
 * @param len Number of data bytes to write
 * @param max_len Maximum allowed data bytes to write
 * @return ELIB_SIMBUS_OK on success
 */
elib_simbus_err_t elib_simbus_i2c_write_mem(
    elib_simbus_i2c_ctx_t *ctx,
    uint8_t dev_addr,
    uint32_t mem_addr,
    uint32_t mem_addr_len,
    const void *data,
    uint32_t len,
    uint32_t max_len);

/**
 * @brief Read from I2C slave memory/register
 * @param ctx I2C context
 * @param dev_addr 7-bit slave address
 * @param mem_addr Memory/register address
 * @param mem_addr_len Number of address bytes (1-4)
 * @param data Buffer to store read data
 * @param len Number of data bytes to read
 * @param max_len Maximum allowed data bytes to read
 * @return ELIB_SIMBUS_OK on success
 */
elib_simbus_err_t elib_simbus_i2c_read_mem(
    elib_simbus_i2c_ctx_t *ctx,
    uint8_t dev_addr,
    uint32_t mem_addr,
    uint32_t mem_addr_len,
    void *data,
    uint32_t len,
    uint32_t max_len);

#ifdef __cplusplus
}
#endif

#endif /* ELIB_SIMBUS_I2C_H */
