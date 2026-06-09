/* test_elib_simbus_uart.c - SimBus UART Unit Tests (State-Machine Driven) */

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

/* ------------------------------------------------------------------ */
/*  RX callback tracking                                                */
/* ------------------------------------------------------------------ */

static uint8_t  rx_cb_byte;
static uint32_t rx_cb_count;

static void mock_rx_callback(elib_simbus_uart_ctx_t *ctx, uint8_t byte)
{
    (void)ctx;
    rx_cb_byte = byte;
    rx_cb_count++;
}

/* ------------------------------------------------------------------ */
/*  Test fixtures                                                       */
/* ------------------------------------------------------------------ */

static elib_simbus_uart_ctx_t ctx;

static void reset_all(void)
{
    memset(&ctx, 0, sizeof(ctx));
    mock_reset_io();
    rx_cb_byte = 0;
    rx_cb_count = 0;
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
        .bit_time_ns = 1000, .data_bits = 8,
        .parity = 0, .stop_bits = 1,
        .io_write = mock_cb_write, .io_read = mock_cb_read,
        .rx_callback = NULL });
    assert(err == ELIB_SIMBUS_OK);
    assert(ctx.bit_flags.initialized == 1);
    assert(ctx.tx_state == ELIB_SIMBUS_UART_STATE_IDLE);
    assert(ctx.rx_state == ELIB_SIMBUS_UART_STATE_IDLE);
}

static void test_init_bit_time_zero(void)
{
    elib_simbus_err_t err = elib_simbus_uart_init(&ctx, &(elib_simbus_uart_cfg_t){
        .tx_pin=0,.rx_pin=1,.bit_time_ns=0,
        .io_write=mock_cb_write,.io_read=mock_cb_read,
        });
    assert(err == ELIB_SIMBUS_ERR_INVALID_PARAM);
}

static void test_init_invalid_data_bits(void)
{
    elib_simbus_err_t err = elib_simbus_uart_init(&ctx, &(elib_simbus_uart_cfg_t){
        .tx_pin=0,.rx_pin=1,.bit_time_ns=1000,.data_bits=4,
        .io_write=mock_cb_write,.io_read=mock_cb_read,
        });
    assert(err == ELIB_SIMBUS_ERR_INVALID_PARAM);
    err = elib_simbus_uart_init(&ctx, &(elib_simbus_uart_cfg_t){
        .tx_pin=0,.rx_pin=1,.bit_time_ns=1000,.data_bits=10,
        .io_write=mock_cb_write,.io_read=mock_cb_read,
        });
    assert(err == ELIB_SIMBUS_ERR_INVALID_PARAM);
}

static void test_init_invalid_parity(void)
{
    elib_simbus_err_t err = elib_simbus_uart_init(&ctx, &(elib_simbus_uart_cfg_t){
        .tx_pin=0,.rx_pin=1,.bit_time_ns=1000,.data_bits=8,.parity=3,
        .io_write=mock_cb_write,.io_read=mock_cb_read,
        });
    assert(err == ELIB_SIMBUS_ERR_INVALID_PARAM);
}

static void test_init_invalid_stop_bits(void)
{
    elib_simbus_err_t err = elib_simbus_uart_init(&ctx, &(elib_simbus_uart_cfg_t){
        .tx_pin=0,.rx_pin=1,.bit_time_ns=1000,.data_bits=8,.stop_bits=3,
        .io_write=mock_cb_write,.io_read=mock_cb_read,
        });
    assert(err == ELIB_SIMBUS_ERR_INVALID_PARAM);
}

static void test_deinit(void)
{
    elib_simbus_uart_init(&ctx, &(elib_simbus_uart_cfg_t){
        .tx_pin=0,.rx_pin=1,.bit_time_ns=1000,.data_bits=8,
        .io_write=mock_cb_write,.io_read=mock_cb_read,
        });
    assert(ctx.bit_flags.initialized == 1);
    elib_simbus_uart_deinit(&ctx);
    assert(ctx.bit_flags.initialized == 0);
    elib_simbus_uart_deinit(NULL);
}

/* ------------------------------------------------------------------ */
/*  TX tests                                                            */
/* ------------------------------------------------------------------ */

static void test_start_tx_null_ctx(void)
{
    uint8_t d = 0x55;
    assert(elib_simbus_uart_start_tx(NULL, &d, 1) == ELIB_SIMBUS_ERR_INVALID_PARAM);
}

static void test_start_tx_not_init(void)
{
    uint8_t d = 0x55;
    assert(elib_simbus_uart_start_tx(&ctx, &d, 1) == ELIB_SIMBUS_ERR_NOT_INITIALIZED);
}

static void test_tx_normal_8n1(void)
{
    elib_simbus_uart_init(&ctx, &(elib_simbus_uart_cfg_t){
        .tx_pin=PIN_TX, .rx_pin=PIN_RX,
        .bit_time_ns=1000, .data_bits=8,
        .io_write=mock_cb_write, .io_read=mock_cb_read,
        .rx_callback=mock_rx_callback });

    uint8_t d = 0xA5;
    assert(elib_simbus_uart_start_tx(&ctx, &d, 1) == ELIB_SIMBUS_OK);
    assert(elib_simbus_uart_tx_busy(&ctx) == 1);
    assert(mock_tx_val == 0); /* start bit = low */

    /* 8N1 frame: start→data0 + data1..7 + data7→stop + stop→idle = 10 polls */
    for (int i = 0; i < 10; i++) {
        elib_simbus_uart_poll_tx(&ctx, 1000);
    }
    assert(elib_simbus_uart_tx_busy(&ctx) == 0);
    assert(mock_tx_val == 1); /* stop bit = high */
}

static void test_tx_busy(void)
{
    elib_simbus_uart_init(&ctx, &(elib_simbus_uart_cfg_t){
        .tx_pin=PIN_TX, .rx_pin=PIN_RX,
        .bit_time_ns=1000, .data_bits=8,
        .io_write=mock_cb_write, .io_read=mock_cb_read,
        });

    uint8_t d1 = 0x55, d2 = 0xAA;
    assert(elib_simbus_uart_start_tx(&ctx, &d1, 1) == ELIB_SIMBUS_OK);
    assert(elib_simbus_uart_start_tx(&ctx, &d2, 1) == ELIB_SIMBUS_ERR_INVALID_PARAM);
}

static void test_tx_parity_even(void)
{
    elib_simbus_uart_init(&ctx, &(elib_simbus_uart_cfg_t){
        .tx_pin=PIN_TX, .rx_pin=PIN_RX,
        .bit_time_ns=1000, .data_bits=8, .parity=2,
        .io_write=mock_cb_write, .io_read=mock_cb_read,
        });

    /* 0x55 = 01010101, even parity = 0 */
    uint8_t d = 0x55;
    assert(elib_simbus_uart_start_tx(&ctx, &d, 1) == ELIB_SIMBUS_OK);
    /* 8E1: start→data0 + data1..7 + data7→parity + parity→stop + stop→idle = 11 polls */
    for (int i = 0; i < 11; i++) {
        elib_simbus_uart_poll_tx(&ctx, 1000);
    }
    assert(elib_simbus_uart_tx_busy(&ctx) == 0);
}

static void test_tx_two_stop_bits(void)
{
    elib_simbus_uart_init(&ctx, &(elib_simbus_uart_cfg_t){
        .tx_pin=PIN_TX, .rx_pin=PIN_RX,
        .bit_time_ns=1000, .data_bits=8, .stop_bits=2,
        .io_write=mock_cb_write, .io_read=mock_cb_read,
        });

    uint8_t d = 0x42;
    assert(elib_simbus_uart_start_tx(&ctx, &d, 1) == ELIB_SIMBUS_OK);
    /* 8N2: start→data0 + data1..7 + data7→stop1 + stop1→stop2 + stop2→idle = 11 polls */
    for (int i = 0; i < 11; i++) {
        elib_simbus_uart_poll_tx(&ctx, 1000);
    }
    assert(elib_simbus_uart_tx_busy(&ctx) == 0);
    assert(mock_tx_val == 1);
}

static void test_tx_buf_normal(void)
{
    elib_simbus_uart_init(&ctx, &(elib_simbus_uart_cfg_t){
        .tx_pin=PIN_TX, .rx_pin=PIN_RX,
        .bit_time_ns=1000, .data_bits=8,
        .io_write=mock_cb_write, .io_read=mock_cb_read,
        });

    uint8_t data[] = {0x12, 0x34, 0x56};
    assert(elib_simbus_uart_start_tx(&ctx, data, 3) == ELIB_SIMBUS_OK);
    assert(elib_simbus_uart_tx_busy(&ctx) == 1);

    /* 3 bytes × 10 bits = 30 polls */
    for (int i = 0; i < 30; i++) {
        elib_simbus_uart_poll_tx(&ctx, 1000);
    }
    assert(elib_simbus_uart_tx_busy(&ctx) == 0);
    assert(mock_tx_val == 1);
}

static void test_tx_buf_single(void)
{
    elib_simbus_uart_init(&ctx, &(elib_simbus_uart_cfg_t){
        .tx_pin=PIN_TX, .rx_pin=PIN_RX,
        .bit_time_ns=1000, .data_bits=8,
        .io_write=mock_cb_write, .io_read=mock_cb_read,
        });

    uint8_t data[] = {0xAA};
    assert(elib_simbus_uart_start_tx(&ctx, data, 1) == ELIB_SIMBUS_OK);

    for (int i = 0; i < 10; i++) {
        elib_simbus_uart_poll_tx(&ctx, 1000);
    }
    assert(elib_simbus_uart_tx_busy(&ctx) == 0);
}

static void test_tx_buf_while_busy(void)
{
    elib_simbus_uart_init(&ctx, &(elib_simbus_uart_cfg_t){
        .tx_pin=PIN_TX, .rx_pin=PIN_RX,
        .bit_time_ns=1000, .data_bits=8,
        .io_write=mock_cb_write, .io_read=mock_cb_read,
        });

    uint8_t data1[] = {0x11};
    uint8_t data2[] = {0x22};
    assert(elib_simbus_uart_start_tx(&ctx, data1, 1) == ELIB_SIMBUS_OK);
    assert(elib_simbus_uart_start_tx(&ctx, data2, 1) == ELIB_SIMBUS_ERR_INVALID_PARAM);
}

/* ------------------------------------------------------------------ */
/*  RX tests                                                            */
/* ------------------------------------------------------------------ */

/**
 * @brief Simulate receiving a UART frame via poll-driven RX
 *
 * Sets mock_rx_val to model the wire state at each bit boundary,
 * then calls poll with bit_time_ns to advance the RX state machine.
 */
static void rx_simulate_frame(uint8_t byte, uint32_t data_bits,
    uint32_t parity, uint32_t stop_bits, uint32_t bit_ns)
{
    /* Build bit sequence: data + [parity] + stop */
    uint8_t bits[16];
    uint32_t nbits = 0;
    uint8_t mask = (uint8_t)((data_bits == 9) ? 0xFF : ((1U << data_bits) - 1));
    byte &= mask;

    for (uint32_t i = 0; i < data_bits; i++) {
        bits[nbits++] = (byte >> i) & 1;
    }
    if (parity) {
        uint8_t p = 0;
        for (uint32_t i = 0; i < data_bits; i++) p ^= (byte >> i) & 1;
        if (parity == 1) p ^= 1;
        bits[nbits++] = p;
    }
    for (uint32_t i = 0; i < stop_bits; i++) {
        bits[nbits++] = 1;
    }

    /* Start: idle */
    mock_rx_val = 1;

    /* Step 1: half-bit advance, no edge yet */
    elib_simbus_uart_poll_rx(&ctx, bit_ns / 2);

    /* Step 2: trigger falling edge (start bit) */
    mock_rx_val = 0;
    elib_simbus_uart_poll_rx(&ctx, bit_ns / 2);

    /* Step 3: remaining half-bit of start bit centering */
    elib_simbus_uart_poll_rx(&ctx, bit_ns / 2);

    /* Step 4..N: one bit_time per data/parity/stop bit */
    for (uint32_t i = 0; i < nbits; i++) {
        mock_rx_val = bits[i];
        elib_simbus_uart_poll_rx(&ctx, bit_ns);
    }
}

static void test_rx_null_ctx(void)
{
    elib_simbus_uart_poll_rx(NULL, 1000);
    /* should not crash */
}

static void test_rx_not_init(void)
{
    elib_simbus_uart_poll_rx(&ctx, 1000);
    /* should not crash */
}

static void test_rx_normal_8n1(void)
{
    elib_simbus_uart_init(&ctx, &(elib_simbus_uart_cfg_t){
        .tx_pin=PIN_TX, .rx_pin=PIN_RX,
        .bit_time_ns=1000, .data_bits=8,
        .io_write=mock_cb_write, .io_read=mock_cb_read,
        .rx_callback=mock_rx_callback });

    rx_simulate_frame(0x4D, 8, 0, 1, 1000);
    assert(rx_cb_count == 1);
    assert(rx_cb_byte == 0x4D);
}

static void test_rx_parity_even(void)
{
    elib_simbus_uart_init(&ctx, &(elib_simbus_uart_cfg_t){
        .tx_pin=PIN_TX, .rx_pin=PIN_RX,
        .bit_time_ns=1000, .data_bits=8, .parity=2, .timeout_ns=100000000,
        .io_write=mock_cb_write, .io_read=mock_cb_read,
        .rx_callback=mock_rx_callback });

    rx_simulate_frame(0x4D, 8, 2, 1, 1000);
    assert(rx_cb_count == 1);
    assert(rx_cb_byte == 0x4D);
}

static void test_rx_parity_odd(void)
{
    elib_simbus_uart_init(&ctx, &(elib_simbus_uart_cfg_t){
        .tx_pin=PIN_TX, .rx_pin=PIN_RX,
        .bit_time_ns=1000, .data_bits=8, .parity=1, .timeout_ns=100000000,
        .io_write=mock_cb_write, .io_read=mock_cb_read,
        .rx_callback=mock_rx_callback });

    rx_simulate_frame(0x4D, 8, 1, 1, 1000);
    assert(rx_cb_count == 1);
    assert(rx_cb_byte == 0x4D);
}

static void test_rx_no_callback(void)
{
    elib_simbus_uart_init(&ctx, &(elib_simbus_uart_cfg_t){
        .tx_pin=PIN_TX, .rx_pin=PIN_RX,
        .bit_time_ns=1000, .data_bits=8,
        .io_write=mock_cb_write, .io_read=mock_cb_read,
        .rx_callback=NULL });

    rx_simulate_frame(0x4D, 8, 0, 1, 1000);
    assert(rx_cb_count == 0); /* no callback registered */
}

static void test_rx_two_bytes(void)
{
    elib_simbus_uart_init(&ctx, &(elib_simbus_uart_cfg_t){
        .tx_pin=PIN_TX, .rx_pin=PIN_RX,
        .bit_time_ns=1000, .data_bits=8,
        .io_write=mock_cb_write, .io_read=mock_cb_read,
        .rx_callback=mock_rx_callback });

    rx_simulate_frame(0xAA, 8, 0, 1, 1000);
    assert(rx_cb_count == 1);
    assert(rx_cb_byte == 0xAA);

    rx_simulate_frame(0x55, 8, 0, 1, 1000);
    assert(rx_cb_count == 2);
    assert(rx_cb_byte == 0x55);
}

/* ------------------------------------------------------------------ */
/*  Main                                                                */
/* ------------------------------------------------------------------ */

int main(void)
{
    printf("=== elib_simbus UART Tests (State-Machine) ===\n");

    printf("\n-- Init / Deinit --\n");
    RUN_TEST(test_init_valid);
    RUN_TEST(test_init_bit_time_zero);
    RUN_TEST(test_init_invalid_data_bits);
    RUN_TEST(test_init_invalid_parity);
    RUN_TEST(test_init_invalid_stop_bits);
    RUN_TEST(test_deinit);

    printf("\n-- TX --\n");
    RUN_TEST(test_start_tx_null_ctx);
    RUN_TEST(test_start_tx_not_init);
    RUN_TEST(test_tx_normal_8n1);
    RUN_TEST(test_tx_busy);
    RUN_TEST(test_tx_parity_even);
    RUN_TEST(test_tx_two_stop_bits);
    RUN_TEST(test_tx_buf_normal);
    RUN_TEST(test_tx_buf_single);
    RUN_TEST(test_tx_buf_while_busy);

    printf("\n-- RX --\n");
    RUN_TEST(test_rx_null_ctx);
    RUN_TEST(test_rx_not_init);
    RUN_TEST(test_rx_normal_8n1);
    RUN_TEST(test_rx_parity_even);
    RUN_TEST(test_rx_parity_odd);
    RUN_TEST(test_rx_no_callback);
    RUN_TEST(test_rx_two_bytes);

    printf("\n=== ALL PASS ===\n");
    return 0;
}
