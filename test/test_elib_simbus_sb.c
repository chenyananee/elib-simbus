/* test_elib_simbus_sb.c - SimBus Single-Bus Protocol Unit Tests */

#include "elib_simbus.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

/* ------------------------------------------------------------------ */
/*  Mock I/O                                                            */
/* ------------------------------------------------------------------ */

#define PIN_BUS 0

static uint8_t mock_pin_val;
static uint8_t mock_pin_dir;

static void mock_reset(void)
{
    mock_pin_val = 1;
    mock_pin_dir = 0;
}

static void mock_cb_write(uint8_t pin, uint8_t level)
{
    if (pin == PIN_BUS) mock_pin_val = level;
}

static uint8_t mock_cb_read(uint8_t pin)
{
    (void)pin;
    return mock_pin_val;
}

static void mock_cb_setdir(uint8_t pin, uint8_t dir)
{
    if (pin == PIN_BUS) mock_pin_dir = dir;
}

/* ------------------------------------------------------------------ */
/*  RX callback tracking                                                */
/* ------------------------------------------------------------------ */

static uint8_t  rx_cb_byte;
static uint32_t rx_cb_count;

static void mock_rx_callback(elib_simbus_sb_ctx_t *ctx, uint8_t byte)
{
    (void)ctx;
    rx_cb_byte = byte;
    rx_cb_count++;
}

/* ------------------------------------------------------------------ */
/*  Test fixtures                                                       */
/* ------------------------------------------------------------------ */

static elib_simbus_sb_ctx_t ctx;

static void reset_all(void)
{
    memset(&ctx, 0, sizeof(ctx));
    mock_reset();
    rx_cb_byte = 0;
    rx_cb_count = 0;
}

#define RUN_TEST(fn) do { \
    reset_all(); \
    fn(); \
    printf("  PASS: %s\n", #fn); \
} while (0)

/* ------------------------------------------------------------------ */
/*  Parity helper                                                       */
/* ------------------------------------------------------------------ */

static uint8_t calc_parity4(uint8_t byte)
{
    uint8_t lo = byte & 0x0F;
    uint8_t hi = (byte >> 4) & 0x0F;
    uint8_t p0 = 0, p1 = 0, p2, p3;
    for (int i = 0; i < 4; i++) {
        p0 ^= (lo >> i) & 1;
        p1 ^= (hi >> i) & 1;
    }
    p2 = ((byte>>0)&1) ^ ((byte>>4)&1) ^ ((byte>>1)&1) ^ ((byte>>5)&1);
    p3 = ((byte>>2)&1) ^ ((byte>>6)&1) ^ ((byte>>3)&1) ^ ((byte>>7)&1);
    return (uint8_t)((p3<<3)|(p2<<2)|(p1<<1)|p0);
}

/* ------------------------------------------------------------------ */
/*  Init tests                                                          */
/* ------------------------------------------------------------------ */

static void test_init_valid(void)
{
    elib_simbus_err_t err = elib_simbus_sb_init(&ctx, &(elib_simbus_sb_cfg_t){
        .pin = PIN_BUS, .bit_time_ns = 1000,
        .io_write = mock_cb_write, .io_read = mock_cb_read,
        .io_setdir = mock_cb_setdir, .rx_callback = NULL });
    assert(err == ELIB_SIMBUS_OK);
    assert(ctx.bit_flags.initialized == 1);
    assert(ctx.state == ELIB_SIMBUS_SB_IDLE);
    assert(mock_pin_dir == ELIB_SIMBUS_SB_DIR_INPUT);
}

static void test_init_bit_time_zero(void)
{
    elib_simbus_err_t err = elib_simbus_sb_init(&ctx, &(elib_simbus_sb_cfg_t){
        .pin=0, .bit_time_ns=0,
        .io_write=mock_cb_write, .io_read=mock_cb_read,
        .io_setdir=mock_cb_setdir });
    assert(err == ELIB_SIMBUS_ERR_INVALID_PARAM);
}

static void test_deinit(void)
{
    elib_simbus_sb_init(&ctx, &(elib_simbus_sb_cfg_t){
        .pin=0, .bit_time_ns=1000,
        .io_write=mock_cb_write, .io_read=mock_cb_read,
        .io_setdir=mock_cb_setdir });
    assert(ctx.bit_flags.initialized == 1);
    elib_simbus_sb_deinit(&ctx);
    assert(ctx.bit_flags.initialized == 0);
    assert(mock_pin_dir == ELIB_SIMBUS_SB_DIR_INPUT);
    elib_simbus_sb_deinit(NULL);
}

/* ------------------------------------------------------------------ */
/*  TX tests                                                            */
/* ------------------------------------------------------------------ */

static void test_start_tx_null(void)
{
    uint8_t d = 0x55;
    assert(elib_simbus_sb_start_tx(NULL, &d, 1) == ELIB_SIMBUS_ERR_INVALID_PARAM);
}

static void test_start_tx_not_init(void)
{
    uint8_t d = 0x55;
    assert(elib_simbus_sb_start_tx(&ctx, &d, 1) == ELIB_SIMBUS_ERR_NOT_INITIALIZED);
}

static void test_tx_single_byte(void)
{
    elib_simbus_sb_init(&ctx, &(elib_simbus_sb_cfg_t){
        .pin=PIN_BUS, .bit_time_ns=1000,
        .io_write=mock_cb_write, .io_read=mock_cb_read,
        .io_setdir=mock_cb_setdir, .rx_callback=mock_rx_callback });

    uint8_t d = 0xA5;
    assert(elib_simbus_sb_start_tx(&ctx, &d, 1) == ELIB_SIMBUS_OK);
    assert(elib_simbus_sb_tx_busy(&ctx) == 1);

    /* 15 idle check + 14 frame bits = 29 polls */
    for (int i = 0; i < 29; i++) {
        elib_simbus_sb_poll(&ctx, 1000);
    }
    assert(elib_simbus_sb_tx_busy(&ctx) == 0);
    assert(mock_pin_val == 1);
    assert(mock_pin_dir == ELIB_SIMBUS_SB_DIR_INPUT);
}

static void test_tx_idle_check_reset(void)
{
    elib_simbus_sb_init(&ctx, &(elib_simbus_sb_cfg_t){
        .pin=PIN_BUS, .bit_time_ns=1000,
        .io_write=mock_cb_write, .io_read=mock_cb_read,
        .io_setdir=mock_cb_setdir });

    uint8_t d = 0x42;
    elib_simbus_sb_start_tx(&ctx, &d, 1);

    for (int i = 0; i < 10; i++) elib_simbus_sb_poll(&ctx, 1000);

    mock_pin_val = 0; /* bus occupied */
    elib_simbus_sb_poll(&ctx, 1000);

    mock_pin_val = 1; /* bus free again */
    for (int i = 0; i < 15; i++) elib_simbus_sb_poll(&ctx, 1000);

    assert(mock_pin_val == 0);
    assert(mock_pin_dir == ELIB_SIMBUS_SB_DIR_OUTPUT);
    assert(elib_simbus_sb_tx_busy(&ctx) == 1);
}

static void test_tx_busy(void)
{
    elib_simbus_sb_init(&ctx, &(elib_simbus_sb_cfg_t){
        .pin=PIN_BUS, .bit_time_ns=1000,
        .io_write=mock_cb_write, .io_read=mock_cb_read,
        .io_setdir=mock_cb_setdir });

    uint8_t d1 = 0x11, d2 = 0x22;
    assert(elib_simbus_sb_start_tx(&ctx, &d1, 1) == ELIB_SIMBUS_OK);
    assert(elib_simbus_sb_start_tx(&ctx, &d2, 1) == ELIB_SIMBUS_ERR_INVALID_PARAM);
}

static void test_tx_multi_byte(void)
{
    elib_simbus_sb_init(&ctx, &(elib_simbus_sb_cfg_t){
        .pin=PIN_BUS, .bit_time_ns=1000,
        .io_write=mock_cb_write, .io_read=mock_cb_read,
        .io_setdir=mock_cb_setdir });

    uint8_t data[] = {0x12, 0x34};
    assert(elib_simbus_sb_start_tx(&ctx, data, 2) == ELIB_SIMBUS_OK);

    /* byte0: 15 idle + 14 frame = 29, byte1: 15 idle + 14 frame = 29 */
    for (int i = 0; i < 58; i++) {
        elib_simbus_sb_poll(&ctx, 1000);
    }
    assert(elib_simbus_sb_tx_busy(&ctx) == 0);
}

/* ------------------------------------------------------------------ */
/*  RX tests                                                            */
/* ------------------------------------------------------------------ */

static void rx_simulate_frame(uint8_t byte, uint32_t bit_ns)
{
    uint8_t parity = calc_parity4(byte);
    uint8_t bits[12];
    for (int i = 0; i < 8; i++) bits[i] = (byte >> i) & 1;
    for (int i = 0; i < 4; i++) bits[8+i] = (parity >> i) & 1;

    mock_pin_val = 1;
    elib_simbus_sb_poll(&ctx, bit_ns / 2);

    mock_pin_val = 0; /* falling edge */
    elib_simbus_sb_poll(&ctx, bit_ns / 2);

    elib_simbus_sb_poll(&ctx, bit_ns / 2); /* start bit centering */

    for (int i = 0; i < 12; i++) {
        mock_pin_val = bits[i];
        elib_simbus_sb_poll(&ctx, bit_ns);
    }

    mock_pin_val = 1; /* stop bit */
    elib_simbus_sb_poll(&ctx, bit_ns);
}

static void test_rx_null(void)
{
    elib_simbus_sb_poll(NULL, 1000);
}

static void test_rx_not_init(void)
{
    elib_simbus_sb_poll(&ctx, 1000);
}

static void test_rx_normal(void)
{
    elib_simbus_sb_init(&ctx, &(elib_simbus_sb_cfg_t){
        .pin=PIN_BUS, .bit_time_ns=1000,
        .io_write=mock_cb_write, .io_read=mock_cb_read,
        .io_setdir=mock_cb_setdir, .rx_callback=mock_rx_callback });

    rx_simulate_frame(0x4D, 1000);
    assert(rx_cb_count == 1);
    assert(rx_cb_byte == 0x4D);
}

static void test_rx_no_callback(void)
{
    elib_simbus_sb_init(&ctx, &(elib_simbus_sb_cfg_t){
        .pin=PIN_BUS, .bit_time_ns=1000,
        .io_write=mock_cb_write, .io_read=mock_cb_read,
        .io_setdir=mock_cb_setdir, .rx_callback=NULL });

    rx_simulate_frame(0x4D, 1000);
    assert(rx_cb_count == 0);
}

static void test_rx_two_frames(void)
{
    elib_simbus_sb_init(&ctx, &(elib_simbus_sb_cfg_t){
        .pin=PIN_BUS, .bit_time_ns=1000,
        .io_write=mock_cb_write, .io_read=mock_cb_read,
        .io_setdir=mock_cb_setdir, .rx_callback=mock_rx_callback });

    rx_simulate_frame(0xAA, 1000);
    assert(rx_cb_count == 1);
    assert(rx_cb_byte == 0xAA);

    rx_simulate_frame(0x55, 1000);
    assert(rx_cb_count == 2);
    assert(rx_cb_byte == 0x55);
}

static void test_rx_busy_blocks_tx(void)
{
    elib_simbus_sb_init(&ctx, &(elib_simbus_sb_cfg_t){
        .pin=PIN_BUS, .bit_time_ns=1000,
        .io_write=mock_cb_write, .io_read=mock_cb_read,
        .io_setdir=mock_cb_setdir, .rx_callback=mock_rx_callback });

    /* Trigger falling edge → enter RX */
    mock_pin_val = 0;
    elib_simbus_sb_poll(&ctx, 1000);
    assert(ctx.state != ELIB_SIMBUS_SB_IDLE);

    /* TX should fail while RX is active */
    uint8_t d = 0x11;
    assert(elib_simbus_sb_start_tx(&ctx, &d, 1) == ELIB_SIMBUS_ERR_INVALID_PARAM);
}

/* ------------------------------------------------------------------ */
/*  Main                                                                */
/* ------------------------------------------------------------------ */

int main(void)
{
    printf("=== elib_simbus SB Tests ===\n");

    printf("\n-- Init / Deinit --\n");
    RUN_TEST(test_init_valid);
    RUN_TEST(test_init_bit_time_zero);
    RUN_TEST(test_deinit);

    printf("\n-- TX --\n");
    RUN_TEST(test_start_tx_null);
    RUN_TEST(test_start_tx_not_init);
    RUN_TEST(test_tx_single_byte);
    RUN_TEST(test_tx_idle_check_reset);
    RUN_TEST(test_tx_busy);
    RUN_TEST(test_tx_multi_byte);

    printf("\n-- RX --\n");
    RUN_TEST(test_rx_null);
    RUN_TEST(test_rx_not_init);
    RUN_TEST(test_rx_normal);
    RUN_TEST(test_rx_no_callback);
    RUN_TEST(test_rx_two_frames);
    RUN_TEST(test_rx_busy_blocks_tx);

    printf("\n=== ALL PASS ===\n");
    return 0;
}
