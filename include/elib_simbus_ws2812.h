/* elib_simbus_ws2812.h - SimBus WS2812 (NeoPixel) Driver */

#ifndef ELIB_SIMBUS_WS2812_H
#define ELIB_SIMBUS_WS2812_H

#include <stdint.h>
#include "elib_simbus_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ------------------------------------------------------------------ */
/*  Callback type definitions                                          */
/* ------------------------------------------------------------------ */

typedef void (*elib_simbus_ws2812_io_write_t)(uint8_t pin, uint8_t level);
typedef uint8_t (*elib_simbus_ws2812_io_read_t)(uint8_t pin);
typedef void (*elib_simbus_ws2812_io_setdir_t)(uint8_t pin, uint8_t dir);
typedef void (*elib_simbus_ws2812_delay_ns_t)(uint32_t ns);

/* ------------------------------------------------------------------ */
/*  Configuration                                                      */
/* ------------------------------------------------------------------ */

typedef struct {
    uint8_t  dq_pin;
    uint32_t t0h;               /* 0-code high time (ns) */
    uint32_t t0l;               /* 0-code low time  (ns) */
    uint32_t t1h;               /* 1-code high time (ns) */
    uint32_t t1l;               /* 1-code low time  (ns) */
    uint32_t reset_ns;          /* reset low time   (ns) */

    elib_simbus_ws2812_io_write_t  io_write;
    elib_simbus_ws2812_io_read_t   io_read;
    elib_simbus_ws2812_io_setdir_t io_setdir;
    elib_simbus_ws2812_delay_ns_t  delay_ns;
} elib_simbus_ws2812_cfg_t;

/* ------------------------------------------------------------------ */
/*  Context                                                            */
/* ------------------------------------------------------------------ */

typedef struct {
    uint8_t  dq_pin;
    uint32_t t0h;
    uint32_t t0l;
    uint32_t t1h;
    uint32_t t1l;
    uint32_t reset_ns;

    elib_simbus_ws2812_io_write_t  io_write;
    elib_simbus_ws2812_io_read_t   io_read;
    elib_simbus_ws2812_io_setdir_t io_setdir;
    elib_simbus_ws2812_delay_ns_t  delay_ns;

    struct {
        uint8_t initialized : 1;
        uint8_t reserved    : 7;
    } bit_flags;
} elib_simbus_ws2812_ctx_t;

/* ------------------------------------------------------------------ */
/*  API                                                                */
/* ------------------------------------------------------------------ */

elib_simbus_err_t elib_simbus_ws2812_init(
    elib_simbus_ws2812_ctx_t *ctx,
    const elib_simbus_ws2812_cfg_t *cfg);

void elib_simbus_ws2812_deinit(elib_simbus_ws2812_ctx_t *ctx);

/**
 * @brief Send raw byte data (each byte is 8 bits, MSB first)
 * @param ctx WS2812 context
 * @param data Byte data to send
 * @param len  Number of bytes
 * @return ELIB_SIMBUS_OK on success
 */
elib_simbus_err_t elib_simbus_ws2812_send(
    elib_simbus_ws2812_ctx_t *ctx,
    const void *data,
    uint32_t len);

/**
 * @brief Send RGB data (auto-converted to GRB order for WS2812)
 * @param ctx WS2812 context
 * @param rgb  RGB888 data (r0,g0,b0, r1,g1,b1, ...)
 * @param count Number of LEDs
 * @return ELIB_SIMBUS_OK on success
 */
elib_simbus_err_t elib_simbus_ws2812_send_rgb(
    elib_simbus_ws2812_ctx_t *ctx,
    const uint8_t *rgb,
    uint32_t count);

#ifdef __cplusplus
}
#endif

#endif /* ELIB_SIMBUS_WS2812_H */
