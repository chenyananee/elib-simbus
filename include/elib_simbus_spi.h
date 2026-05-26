/* elib_simbus_spi.h - SimBus SPI Master (Bit-Bang) */

#ifndef ELIB_SIMBUS_SPI_H
#define ELIB_SIMBUS_SPI_H

#include <stdint.h>
#include "elib_simbus_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ------------------------------------------------------------------ */
/*  Callback type definitions                                          */
/* ------------------------------------------------------------------ */

typedef void (*elib_simbus_spi_io_write_t)(uint8_t pin, uint8_t level);
typedef uint8_t (*elib_simbus_spi_io_read_t)(uint8_t pin);
typedef void (*elib_simbus_spi_io_setdir_t)(uint8_t pin, uint8_t dir);
typedef void (*elib_simbus_spi_delay_us_t)(uint32_t us);

/* ------------------------------------------------------------------ */
/*  Configuration                                                      */
/* ------------------------------------------------------------------ */

#define ELIB_SIMBUS_SPI_DEFAULT_DUMMY 0x00

typedef struct {
    uint8_t  sclk_pin;
    uint8_t  mosi_pin;
    uint8_t  miso_pin;
    uint8_t  cs_pin;
    uint32_t delay_num;
    uint32_t mode;              /* 0-3 */
    uint32_t bit_order;         /* 0=MSB first, 1=LSB first */
    uint8_t  dummy_byte;        /* sent when tx_data is NULL */

    elib_simbus_spi_io_write_t  io_write;
    elib_simbus_spi_io_read_t   io_read;
    elib_simbus_spi_io_setdir_t io_setdir;
    elib_simbus_spi_delay_us_t  delay_us;
} elib_simbus_spi_cfg_t;

/* ------------------------------------------------------------------ */
/*  Context                                                            */
/* ------------------------------------------------------------------ */

typedef struct {
    uint8_t  sclk_pin;
    uint8_t  mosi_pin;
    uint8_t  miso_pin;
    uint8_t  cs_pin;
    uint32_t delay_num;
    uint32_t mode;
    uint32_t bit_order;
    uint8_t  dummy_byte;

    elib_simbus_spi_io_write_t  io_write;
    elib_simbus_spi_io_read_t   io_read;
    elib_simbus_spi_io_setdir_t io_setdir;
    elib_simbus_spi_delay_us_t  delay_us;

    struct {
        uint8_t initialized : 1;
        uint8_t reserved    : 7;
    } bit_flags;
} elib_simbus_spi_ctx_t;

/* ------------------------------------------------------------------ */
/*  API                                                                */
/* ------------------------------------------------------------------ */

elib_simbus_err_t elib_simbus_spi_init(
    elib_simbus_spi_ctx_t *ctx,
    const elib_simbus_spi_cfg_t *cfg);

void elib_simbus_spi_deinit(elib_simbus_spi_ctx_t *ctx);

/**
 * @brief Full-duplex SPI transfer
 * @param ctx SPI context
 * @param tx_data Data to send (NULL to send dummy bytes)
 * @param rx_data Buffer for received data (NULL to discard)
 * @param len Number of bytes to transfer
 * @param max_len Maximum allowed bytes
 * @return ELIB_SIMBUS_OK on success
 */
elib_simbus_err_t elib_simbus_spi_transfer(
    elib_simbus_spi_ctx_t *ctx,
    const void *tx_data,
    void *rx_data,
    uint32_t len,
    uint32_t max_len);

/**
 * @brief SPI write only (tx_data sent, rx discarded)
 */
elib_simbus_err_t elib_simbus_spi_write(
    elib_simbus_spi_ctx_t *ctx,
    const void *data,
    uint32_t len,
    uint32_t max_len);

/**
 * @brief SPI read only (dummy bytes sent, rx_data received)
 */
elib_simbus_err_t elib_simbus_spi_read(
    elib_simbus_spi_ctx_t *ctx,
    void *data,
    uint32_t len,
    uint32_t max_len);

/**
 * @brief Assert chip select (pull low by default)
 */
void elib_simbus_spi_cs_low(elib_simbus_spi_ctx_t *ctx);

/**
 * @brief De-assert chip select (pull high by default)
 */
void elib_simbus_spi_cs_high(elib_simbus_spi_ctx_t *ctx);

#ifdef __cplusplus
}
#endif

#endif /* ELIB_SIMBUS_SPI_H */
