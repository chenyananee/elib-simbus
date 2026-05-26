/* test_elib_simbus_uart.c - SimBus UART Unit Tests */

#include "elib_simbus.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

/* ------------------------------------------------------------------ */
/*  Mock I/O                                                            */
/* ------------------------------------------------------------------ */

#define PIN_TX  0
#define PIN_RX  1

static uint8_t mock_tx_val;
static uint8_t mock_rx_val;

static void mock_reset_io(void)
{
    mock_tx_val = 1;
    mock_rx_val = 1;
}

static void mock_cb_write(uint8_t pin, uint8_t level)
{
    if (pin == PIN_TX) mock_tx_val = level;
}

static uint8_t mock_cb_read(uint8_t pin)
{
    (void)pin;
    return mock_rx_val;
}

static void mock_cb_setdir(uint8_t pin, uint8_t dir)
{
    (void)pin; (void)dir;
}

static void mock_cb_delay(uint32_t us)
{
    (void)us;
}

/* ------------------------------------------------------------------ */
/*  RX playback for getchar tests                                       */
/*  Delay callback advances a frame through mock_rx_val                */
/* ------------------------------------------------------------------ */

static uint8_t rx_frame[32];
static uint32_t rx_frame_len;
static uint32_t rx_timer;       /* increments on every delay_us call */

/*
 * rx_timer mapping for getchar (8N1):
 *   0       : initial (mock_rx_val = 1 = idle)
 *   1       : 1st delay → mock_rx_val = 0 (start bit)
 *   2       : half-bit delay → no change
 *   3..10   : 8 data delays → rx_frame[0..7]
 *   11      : stop delay → rx_frame[8] = 1
 */
static void rx_delay_tick(uint32_t us)
{
    (void)us;
    rx_timer++;

    if (rx_timer == 1) {
        mock_rx_val = 0;                  /* start bit */
    } else if (rx_timer >= 3) {
        uint32_t idx = rx_timer - 3;      /* 0 = first data bit */
        if (idx < rx_frame_len) {
            mock_rx_val = rx_frame[idx];
        } else {
            mock_rx_val = 1;              /* idle */
        }
    }
}

static void rx_build_frame(uint8_t byte, uint32_t data_bits,
    uint32_t parity, uint32_t stop_bits)
{
    rx_frame_len = 0;
    uint8_t mask = (uint8_t)((data_bits == 9) ? 0xFF : ((1U << data_bits) - 1));
    byte &= mask;

    for (uint32_t i = 0; i < data_bits; i++) {
        rx_frame[rx_frame_len++] = (byte >> i) & 1;
    }

    if (parity) {
        uint8_t p = 0;
        for (uint32_t i = 0; i < data_bits; i++) p ^= (byte >> i) & 1;
        if (parity == 1) p ^= 1;
        rx_frame[rx_frame_len++] = p;
    }

    for (uint32_t i = 0; i < stop_bits; i++) {
        rx_frame[rx_frame_len++] = 1;
    }
}

static void rx_setup_frame(uint8_t byte, uint32_t data_bits,
    uint32_t parity, uint32_t stop_bits)
{
    rx_timer = 0;
    rx_build_frame(byte, data_bits, parity, stop_bits);
    mock_rx_val = 1;  /* idle */
}

/* ------------------------------------------------------------------ */
/*  Test fixtures                                                       */
/* ------------------------------------------------------------------ */

static elib_simbus_uart_ctx_t ctx;

static void reset_all(void)
{
    memset(&ctx, 0, sizeof(ctx));
    mock_reset_io();
    rx_timer = 0;
}

#define RUN_TEST(fn) do { \
    reset_all(); \
    fn(); \
    printf("  PASS: %s\n", #fn); \
} while (0)

/* ------------------------------------------------------------------ */
/*  Init tests                                                          */
/* ------------------------------------------------------------------ */

static void test_init_valid(void)
{
    elib_simbus_err_t err = elib_simbus_uart_init(&ctx, &(elib_simbus_uart_cfg_t){
        .tx_pin = PIN_TX, .rx_pin = PIN_RX,
        .bit_time_us = 100, .data_bits = 8,
        .parity = 0, .stop_bits = 1,
        .io_write = mock_cb_write, .io_read = mock_cb_read,
        .io_setdir = mock_cb_setdir, .delay_us = mock_cb_delay });
    assert(err == ELIB_SIMBUS_OK);
    assert(ctx.bit_flags.initialized == 1);
}

static void test_init_bit_time_zero(void)
{
    elib_simbus_err_t err = elib_simbus_uart_init(&ctx, &(elib_simbus_uart_cfg_t){
        .tx_pin=0,.rx_pin=1,.bit_time_us=0,
        .io_write=mock_cb_write,.io_read=mock_cb_read,
        .io_setdir=mock_cb_setdir,.delay_us=mock_cb_delay });
    assert(err == ELIB_SIMBUS_ERR_INVALID_PARAM);
}

static void test_init_invalid_data_bits(void)
{
    elib_simbus_err_t err = elib_simbus_uart_init(&ctx, &(elib_simbus_uart_cfg_t){
        .tx_pin=0,.rx_pin=1,.bit_time_us=100,.data_bits=4,
        .io_write=mock_cb_write,.io_read=mock_cb_read,
        .io_setdir=mock_cb_setdir,.delay_us=mock_cb_delay });
    assert(err == ELIB_SIMBUS_ERR_INVALID_PARAM);
    err = elib_simbus_uart_init(&ctx, &(elib_simbus_uart_cfg_t){
        .tx_pin=0,.rx_pin=1,.bit_time_us=100,.data_bits=10,
        .io_write=mock_cb_write,.io_read=mock_cb_read,
        .io_setdir=mock_cb_setdir,.delay_us=mock_cb_delay });
    assert(err == ELIB_SIMBUS_ERR_INVALID_PARAM);
}

static void test_init_invalid_parity(void)
{
    elib_simbus_err_t err = elib_simbus_uart_init(&ctx, &(elib_simbus_uart_cfg_t){
        .tx_pin=0,.rx_pin=1,.bit_time_us=100,.data_bits=8,.parity=3,
        .io_write=mock_cb_write,.io_read=mock_cb_read,
        .io_setdir=mock_cb_setdir,.delay_us=mock_cb_delay });
    assert(err == ELIB_SIMBUS_ERR_INVALID_PARAM);
}

static void test_init_invalid_stop_bits(void)
{
    elib_simbus_err_t err = elib_simbus_uart_init(&ctx, &(elib_simbus_uart_cfg_t){
        .tx_pin=0,.rx_pin=1,.bit_time_us=100,.data_bits=8,.stop_bits=3,
        .io_write=mock_cb_write,.io_read=mock_cb_read,
        .io_setdir=mock_cb_setdir,.delay_us=mock_cb_delay });
    assert(err == ELIB_SIMBUS_ERR_INVALID_PARAM);
}

static void test_deinit(void)
{
    elib_simbus_uart_init(&ctx, &(elib_simbus_uart_cfg_t){
        .tx_pin=0,.rx_pin=1,.bit_time_us=100,.data_bits=8,
        .io_write=mock_cb_write,.io_read=mock_cb_read,
        .io_setdir=mock_cb_setdir,.delay_us=mock_cb_delay });
    assert(ctx.bit_flags.initialized == 1);
    elib_simbus_uart_deinit(&ctx);
    assert(ctx.bit_flags.initialized == 0);
    elib_simbus_uart_deinit(NULL);
}

/* ------------------------------------------------------------------ */
/*  Putchar tests                                                       */
/* ------------------------------------------------------------------ */

static void test_putchar_null_ctx(void)
{
    assert(elib_simbus_uart_putchar(NULL, 0x55) == ELIB_SIMBUS_ERR_INVALID_PARAM);
}

static void test_putchar_not_init(void)
{
    assert(elib_simbus_uart_putchar(&ctx, 0x55) == ELIB_SIMBUS_ERR_NOT_INITIALIZED);
}

static void test_putchar_normal(void)
{
    elib_simbus_uart_init(&ctx, &(elib_simbus_uart_cfg_t){
        .tx_pin=PIN_TX, .rx_pin=PIN_RX,
        .bit_time_us=1, .data_bits=8,
        .io_write=mock_cb_write, .io_read=mock_cb_read,
        .io_setdir=mock_cb_setdir, .delay_us=mock_cb_delay });
    assert(elib_simbus_uart_putchar(&ctx, 0xA5) == ELIB_SIMBUS_OK);
    assert(mock_tx_val == 1);
}

/* ------------------------------------------------------------------ */
/*  Getchar tests                                                       */
/* ------------------------------------------------------------------ */

static void test_getchar_null_ctx(void)
{
    assert(elib_simbus_uart_getchar(NULL) == -1);
}

static void test_getchar_not_init(void)
{
    assert(elib_simbus_uart_getchar(&ctx) == -1);
}

static void test_getchar_normal(void)
{
    elib_simbus_uart_init(&ctx, &(elib_simbus_uart_cfg_t){
        .tx_pin=PIN_TX, .rx_pin=PIN_RX,
        .bit_time_us=1, .data_bits=8, .timeout_rounds=1000,
        .io_write=mock_cb_write, .io_read=mock_cb_read,
        .io_setdir=mock_cb_setdir, .delay_us=rx_delay_tick });

    rx_setup_frame(0x4D, 8, 0, 1);
    int32_t byte = elib_simbus_uart_getchar(&ctx);
    assert(byte == 0x4D);
}

static void test_getchar_parity_even(void)
{
    elib_simbus_uart_init(&ctx, &(elib_simbus_uart_cfg_t){
        .tx_pin=PIN_TX, .rx_pin=PIN_RX,
        .bit_time_us=1, .data_bits=8, .parity=2, .timeout_rounds=1000,
        .io_write=mock_cb_write, .io_read=mock_cb_read,
        .io_setdir=mock_cb_setdir, .delay_us=rx_delay_tick });

    rx_setup_frame(0x4D, 8, 2, 1);
    int32_t byte = elib_simbus_uart_getchar(&ctx);
    assert(byte == 0x4D);
}

static void test_getchar_parity_odd(void)
{
    elib_simbus_uart_init(&ctx, &(elib_simbus_uart_cfg_t){
        .tx_pin=PIN_TX, .rx_pin=PIN_RX,
        .bit_time_us=1, .data_bits=8, .parity=1, .timeout_rounds=1000,
        .io_write=mock_cb_write, .io_read=mock_cb_read,
        .io_setdir=mock_cb_setdir, .delay_us=rx_delay_tick });

    rx_setup_frame(0x4D, 8, 1, 1);
    int32_t byte = elib_simbus_uart_getchar(&ctx);
    assert(byte == 0x4D);
}

static void test_getchar_timeout(void)
{
    elib_simbus_uart_init(&ctx, &(elib_simbus_uart_cfg_t){
        .tx_pin=PIN_TX, .rx_pin=PIN_RX,
        .bit_time_us=1, .data_bits=8, .timeout_rounds=3,
        .io_write=mock_cb_write, .io_read=mock_cb_read,
        .io_setdir=mock_cb_setdir, .delay_us=mock_cb_delay });

    mock_rx_val = 1;
    int32_t byte = elib_simbus_uart_getchar(&ctx);
    assert(byte == -1);
    assert(ctx.bit_flags.timeout == 1);
}

/* ------------------------------------------------------------------ */
/*  Write / read tests                                                  */
/* ------------------------------------------------------------------ */

static void test_write_normal(void)
{
    elib_simbus_uart_init(&ctx, &(elib_simbus_uart_cfg_t){
        .tx_pin=PIN_TX, .rx_pin=PIN_RX,
        .bit_time_us=1, .data_bits=8,
        .io_write=mock_cb_write, .io_read=mock_cb_read,
        .io_setdir=mock_cb_setdir, .delay_us=mock_cb_delay });

    uint8_t data[] = {0x12, 0x34, 0x56};
    assert(elib_simbus_uart_write(&ctx, data, 3, 3) == ELIB_SIMBUS_OK);
    assert(mock_tx_val == 1);
}

static void test_write_exceed_max(void)
{
    elib_simbus_uart_init(&ctx, &(elib_simbus_uart_cfg_t){
        .tx_pin=PIN_TX, .rx_pin=PIN_RX,
        .bit_time_us=1, .data_bits=8,
        .io_write=mock_cb_write, .io_read=mock_cb_read,
        .io_setdir=mock_cb_setdir, .delay_us=mock_cb_delay });

    uint8_t data[4] = {0};
    assert(elib_simbus_uart_write(&ctx, data, 4, 2) == ELIB_SIMBUS_ERR_EXCEED_MAX);
}

static void test_read_timeout(void)
{
    elib_simbus_uart_init(&ctx, &(elib_simbus_uart_cfg_t){
        .tx_pin=PIN_TX, .rx_pin=PIN_RX,
        .bit_time_us=1, .data_bits=8, .timeout_rounds=3,
        .io_write=mock_cb_write, .io_read=mock_cb_read,
        .io_setdir=mock_cb_setdir, .delay_us=mock_cb_delay });

    mock_rx_val = 1;
    uint8_t buf[4] = {0};
    int32_t n = elib_simbus_uart_read(&ctx, buf, 4, 4);
    assert(n == 0);
}

/* ------------------------------------------------------------------ */
/*  Main                                                                */
/* ------------------------------------------------------------------ */

int main(void)
{
    printf("=== elib_simbus UART Tests ===\n");

    printf("\n-- Init / Deinit --\n");
    RUN_TEST(test_init_valid);
    RUN_TEST(test_init_bit_time_zero);
    RUN_TEST(test_init_invalid_data_bits);
    RUN_TEST(test_init_invalid_parity);
    RUN_TEST(test_init_invalid_stop_bits);
    RUN_TEST(test_deinit);

    printf("\n-- Putchar --\n");
    RUN_TEST(test_putchar_null_ctx);
    RUN_TEST(test_putchar_not_init);
    RUN_TEST(test_putchar_normal);

    printf("\n-- Getchar --\n");
    RUN_TEST(test_getchar_null_ctx);
    RUN_TEST(test_getchar_not_init);
    RUN_TEST(test_getchar_normal);
    RUN_TEST(test_getchar_parity_even);
    RUN_TEST(test_getchar_parity_odd);
    RUN_TEST(test_getchar_timeout);

    printf("\n-- Write / Read --\n");
    RUN_TEST(test_write_normal);
    RUN_TEST(test_write_exceed_max);
    RUN_TEST(test_read_timeout);

    printf("\n=== ALL PASS ===\n");
    return 0;
}
