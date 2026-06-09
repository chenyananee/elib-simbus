/* elib_simbus_sb.h - SimBus Single-Bus Protocol (State-Machine Driven) */

#ifndef ELIB_SIMBUS_SB_H
#define ELIB_SIMBUS_SB_H

#include <stdint.h>
#include "elib_simbus_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ------------------------------------------------------------------ */
/*  Frame format: START(low) + D0..D7 + P0..P3 + STOP(high)
 *  P0=even(D0..D3), P1=even(D4..D7)
 *  P2=even(D0,D4,D1,D5), P3=even(D2,D6,D3,D7)                       */
/* ------------------------------------------------------------------ */

/* ------------------------------------------------------------------ */
/*  Callback type definitions                                          */
/* ------------------------------------------------------------------ */

typedef void (*elib_simbus_sb_io_write_t)(uint8_t pin, uint8_t level);
typedef uint8_t (*elib_simbus_sb_io_read_t)(uint8_t pin);
typedef void (*elib_simbus_sb_io_setdir_t)(uint8_t pin, uint8_t dir);

#define ELIB_SIMBUS_SB_DIR_INPUT  0
#define ELIB_SIMBUS_SB_DIR_OUTPUT 1

/**
 * @brief RX byte received callback
 * @param ctx  SB context
 * @param byte Received data byte
 */
typedef void (*elib_simbus_sb_rx_callback_t)(
    struct elib_simbus_sb_ctx *ctx, uint8_t byte);

/* ------------------------------------------------------------------ */
/*  State enum (TX/RX combined, mutually exclusive)                     */
/* ------------------------------------------------------------------ */

typedef enum {
    ELIB_SIMBUS_SB_IDLE = 0,

    /* TX states */
    ELIB_SIMBUS_SB_TX_IDLE_CHECK,
    ELIB_SIMBUS_SB_TX_START_BIT,
    ELIB_SIMBUS_SB_TX_DATA_BIT,
    ELIB_SIMBUS_SB_TX_PARITY_BIT,
    ELIB_SIMBUS_SB_TX_STOP_BIT,

    /* RX states */
    ELIB_SIMBUS_SB_RX_START_BIT,
    ELIB_SIMBUS_SB_RX_DATA_BIT,
    ELIB_SIMBUS_SB_RX_PARITY_BIT,
    ELIB_SIMBUS_SB_RX_STOP_BIT,
} elib_simbus_sb_state_t;

/* ------------------------------------------------------------------ */
/*  Configuration                                                      */
/* ------------------------------------------------------------------ */

typedef struct {
    uint8_t  pin;                   /* 半双工总线引脚 */
    uint32_t bit_time_ns;           /* 位时间 (ns) */

    elib_simbus_sb_io_write_t  io_write;
    elib_simbus_sb_io_read_t   io_read;
    elib_simbus_sb_io_setdir_t io_setdir;
    elib_simbus_sb_rx_callback_t rx_callback;
} elib_simbus_sb_cfg_t;

/* ------------------------------------------------------------------ */
/*  Context                                                            */
/* ------------------------------------------------------------------ */

typedef struct elib_simbus_sb_ctx {
    uint8_t  pin;
    uint32_t bit_time_ns;

    elib_simbus_sb_io_write_t  io_write;
    elib_simbus_sb_io_read_t   io_read;
    elib_simbus_sb_io_setdir_t io_setdir;
    elib_simbus_sb_rx_callback_t rx_callback;

    /* Combined state machine */
    elib_simbus_sb_state_t state;
    uint8_t  byte;              /* 当前收发的字节 */
    uint8_t  parity;            /* 4-bit parity */
    uint8_t  bit_idx;           /* 位索引 */
    uint32_t acc_ns;            /* 时间累加器 */
    uint8_t  prev_level;        /* 上次引脚电平 (RX 下降沿检测) */

    /* TX buffer */
    const uint8_t *tx_buf;
    uint32_t tx_buf_len;
    uint32_t tx_buf_idx;
    int32_t  tx_idle_countdown; /* 空闲检测倒计时 (ns) */

    struct {
        uint8_t initialized : 1;
        uint8_t reserved    : 7;
    } bit_flags;
} elib_simbus_sb_ctx_t;

/* ------------------------------------------------------------------ */
/*  API                                                                */
/* ------------------------------------------------------------------ */

elib_simbus_err_t elib_simbus_sb_init(
    elib_simbus_sb_ctx_t *ctx,
    const elib_simbus_sb_cfg_t *cfg);

void elib_simbus_sb_deinit(elib_simbus_sb_ctx_t *ctx);

/**
 * @brief Start transmitting a byte array
 *        Checks bus idle for 15 bit times first, then sends frame(s)
 *        RX is disabled while TX is active
 */
elib_simbus_err_t elib_simbus_sb_start_tx(
    elib_simbus_sb_ctx_t *ctx, const uint8_t *data, uint32_t len);

/**
 * @brief Poll the state machine, advancing at most one step
 *        Handles both TX and RX (mutually exclusive)
 * @param elapsed_ns Nanoseconds since last poll
 */
void elib_simbus_sb_poll(
    elib_simbus_sb_ctx_t *ctx, uint32_t elapsed_ns);

/**
 * @brief Check if TX is in progress
 * @return 1 if busy, 0 if idle
 */
uint8_t elib_simbus_sb_tx_busy(elib_simbus_sb_ctx_t *ctx);

#ifdef __cplusplus
}
#endif

#endif /* ELIB_SIMBUS_SB_H */
