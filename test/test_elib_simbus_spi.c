/* test_elib_simbus_spi.c - SimBus SPI Unit Tests */

#include "elib_simbus.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

/* ------------------------------------------------------------------ */
/*  Mock I/O                                                            */
/* ------------------------------------------------------------------ */

#define PIN_SCLK  0
#define PIN_MOSI  1
#define PIN_MISO  2
#define PIN_CS    3

static uint8_t mock_sclk, mock_mosi, mock_cs;
static uint8_t mock_miso_in;     /* value io_read returns on MISO */

/* Track what the master sent on MOSI per byte */
static void mock_reset_io(void)
{
    mock_sclk = 0; mock_mosi = 0; mock_cs = 1;
    mock_miso_in = 0xFF;
}

static void mock_cb_write(uint8_t pin, uint8_t level)
{
    if (pin == PIN_SCLK) mock_sclk = level;
    else if (pin == PIN_MOSI) mock_mosi = level;
    else if (pin == PIN_CS) mock_cs = level;
}

static uint8_t mock_cb_read(uint8_t pin)
{
    (void)pin;
    return (mock_miso_in != 0) ? 1 : 0;
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
/*  Test fixtures                                                       */
/* ------------------------------------------------------------------ */

static elib_simbus_spi_ctx_t ctx;

static void reset_all(void)
{
    memset(&ctx, 0, sizeof(ctx));
    mock_reset_io();
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
    elib_simbus_err_t err = elib_simbus_spi_init(&ctx, &(elib_simbus_spi_cfg_t){
        .sclk_pin = PIN_SCLK, .mosi_pin = PIN_MOSI,
        .miso_pin = PIN_MISO, .cs_pin = PIN_CS,
        .delay_num = 1, .mode = 0,
        .io_write = mock_cb_write, .io_read = mock_cb_read,
        .io_setdir = mock_cb_setdir, .delay_us = mock_cb_delay });
    assert(err == ELIB_SIMBUS_OK);
    assert(ctx.bit_flags.initialized == 1);
    assert(ctx.mode == 0);
}

static void test_init_null_ctx(void)
{
    elib_simbus_err_t err = elib_simbus_spi_init(NULL, &(elib_simbus_spi_cfg_t){
        .sclk_pin = 0, .mosi_pin = 1, .miso_pin = 2, .cs_pin = 3,
        .delay_num = 1, .mode = 0,
        .io_write = mock_cb_write, .io_read = mock_cb_read,
        .io_setdir = mock_cb_setdir, .delay_us = mock_cb_delay });
    assert(err == ELIB_SIMBUS_ERR_INVALID_PARAM);
}

static void test_init_null_cfg(void)
{
    assert(elib_simbus_spi_init(&ctx, NULL) == ELIB_SIMBUS_ERR_INVALID_PARAM);
}

static void test_init_null_callbacks(void)
{
    elib_simbus_err_t err;
    err = elib_simbus_spi_init(&ctx, &(elib_simbus_spi_cfg_t){
        .sclk_pin=0,.mosi_pin=1,.miso_pin=2,.cs_pin=3,
        .delay_num=1,.mode=0,
        .io_write=NULL,.io_read=mock_cb_read,
        .io_setdir=mock_cb_setdir,.delay_us=mock_cb_delay });
    assert(err == ELIB_SIMBUS_ERR_INVALID_PARAM);
    err = elib_simbus_spi_init(&ctx, &(elib_simbus_spi_cfg_t){
        .sclk_pin=0,.mosi_pin=1,.miso_pin=2,.cs_pin=3,
        .delay_num=1,.mode=0,
        .io_write=mock_cb_write,.io_read=NULL,
        .io_setdir=mock_cb_setdir,.delay_us=mock_cb_delay });
    assert(err == ELIB_SIMBUS_ERR_INVALID_PARAM);
    err = elib_simbus_spi_init(&ctx, &(elib_simbus_spi_cfg_t){
        .sclk_pin=0,.mosi_pin=1,.miso_pin=2,.cs_pin=3,
        .delay_num=1,.mode=0,
        .io_write=mock_cb_write,.io_read=mock_cb_read,
        .io_setdir=NULL,.delay_us=mock_cb_delay });
    assert(err == ELIB_SIMBUS_ERR_INVALID_PARAM);
    err = elib_simbus_spi_init(&ctx, &(elib_simbus_spi_cfg_t){
        .sclk_pin=0,.mosi_pin=1,.miso_pin=2,.cs_pin=3,
        .delay_num=1,.mode=0,
        .io_write=mock_cb_write,.io_read=mock_cb_read,
        .io_setdir=mock_cb_setdir,.delay_us=NULL });
    assert(err == ELIB_SIMBUS_ERR_INVALID_PARAM);
}

static void test_init_invalid_mode(void)
{
    elib_simbus_err_t err = elib_simbus_spi_init(&ctx, &(elib_simbus_spi_cfg_t){
        .sclk_pin=0,.mosi_pin=1,.miso_pin=2,.cs_pin=3,
        .delay_num=1,.mode=4,
        .io_write=mock_cb_write,.io_read=mock_cb_read,
        .io_setdir=mock_cb_setdir,.delay_us=mock_cb_delay });
    assert(err == ELIB_SIMBUS_ERR_INVALID_PARAM);
}

static void test_deinit(void)
{
    elib_simbus_spi_init(&ctx, &(elib_simbus_spi_cfg_t){
        .sclk_pin=0,.mosi_pin=1,.miso_pin=2,.cs_pin=3,
        .delay_num=1,.mode=0,
        .io_write=mock_cb_write,.io_read=mock_cb_read,
        .io_setdir=mock_cb_setdir,.delay_us=mock_cb_delay });
    assert(ctx.bit_flags.initialized == 1);
    elib_simbus_spi_deinit(&ctx);
    assert(ctx.bit_flags.initialized == 0);
    elib_simbus_spi_deinit(NULL);
}

/* ------------------------------------------------------------------ */
/*  Parameter validation tests                                          */
/* ------------------------------------------------------------------ */

static void test_transfer_null_ctx(void)
{
    uint8_t d = 0;
    assert(elib_simbus_spi_transfer(NULL, &d, &d, 1, 1) == ELIB_SIMBUS_ERR_INVALID_PARAM);
}

static void test_transfer_not_init(void)
{
    uint8_t d = 0;
    assert(elib_simbus_spi_transfer(&ctx, &d, &d, 1, 1) == ELIB_SIMBUS_ERR_NOT_INITIALIZED);
}

static void test_transfer_exceed_max(void)
{
    elib_simbus_spi_init(&ctx, &(elib_simbus_spi_cfg_t){
        .sclk_pin=0,.mosi_pin=1,.miso_pin=2,.cs_pin=3,
        .delay_num=1,.mode=0,
        .io_write=mock_cb_write,.io_read=mock_cb_read,
        .io_setdir=mock_cb_setdir,.delay_us=mock_cb_delay });
    uint8_t d[4] = {0};
    assert(elib_simbus_spi_transfer(&ctx, d, d, 4, 2) == ELIB_SIMBUS_ERR_EXCEED_MAX);
}

static void test_transfer_zero_len(void)
{
    elib_simbus_spi_init(&ctx, &(elib_simbus_spi_cfg_t){
        .sclk_pin=0,.mosi_pin=1,.miso_pin=2,.cs_pin=3,
        .delay_num=1,.mode=0,
        .io_write=mock_cb_write,.io_read=mock_cb_read,
        .io_setdir=mock_cb_setdir,.delay_us=mock_cb_delay });
    uint8_t d = 0;
    assert(elib_simbus_spi_transfer(&ctx, &d, &d, 0, 1) == ELIB_SIMBUS_OK);
}

/* ------------------------------------------------------------------ */
/*  Functional tests (Mode 0)                                           */
/* ------------------------------------------------------------------ */

static void test_mode0_transfer(void)
{
    elib_simbus_spi_init(&ctx, &(elib_simbus_spi_cfg_t){
        .sclk_pin=PIN_SCLK, .mosi_pin=PIN_MOSI,
        .miso_pin=PIN_MISO, .cs_pin=PIN_CS,
        .delay_num=1, .mode=0,
        .io_write=mock_cb_write, .io_read=mock_cb_read,
        .io_setdir=mock_cb_setdir, .delay_us=mock_cb_delay });

    uint8_t tx[] = {0x12, 0x34, 0x56};
    uint8_t rx[3] = {0};
    assert(elib_simbus_spi_transfer(&ctx, tx, rx, 3, 3) == ELIB_SIMBUS_OK);
}

static void test_mode0_write(void)
{
    elib_simbus_spi_init(&ctx, &(elib_simbus_spi_cfg_t){
        .sclk_pin=PIN_SCLK, .mosi_pin=PIN_MOSI,
        .miso_pin=PIN_MISO, .cs_pin=PIN_CS,
        .delay_num=1, .mode=0,
        .io_write=mock_cb_write, .io_read=mock_cb_read,
        .io_setdir=mock_cb_setdir, .delay_us=mock_cb_delay });

    uint8_t tx[] = {0xAB, 0xCD};
    assert(elib_simbus_spi_write(&ctx, tx, 2, 2) == ELIB_SIMBUS_OK);
}

static void test_mode0_read(void)
{
    elib_simbus_spi_init(&ctx, &(elib_simbus_spi_cfg_t){
        .sclk_pin=PIN_SCLK, .mosi_pin=PIN_MOSI,
        .miso_pin=PIN_MISO, .cs_pin=PIN_CS,
        .delay_num=1, .mode=0,
        .io_write=mock_cb_write, .io_read=mock_cb_read,
        .io_setdir=mock_cb_setdir, .delay_us=mock_cb_delay });

    mock_miso_in = 0x01;  /* MISO always high → rx byte = 0xFF */

    uint8_t rx[2] = {0};
    assert(elib_simbus_spi_read(&ctx, rx, 2, 2) == ELIB_SIMBUS_OK);
    assert(rx[0] == 0xFF);
    assert(rx[1] == 0xFF);
}

static void test_cs_helpers(void)
{
    elib_simbus_spi_init(&ctx, &(elib_simbus_spi_cfg_t){
        .sclk_pin=PIN_SCLK, .mosi_pin=PIN_MOSI,
        .miso_pin=PIN_MISO, .cs_pin=PIN_CS,
        .delay_num=1, .mode=0,
        .io_write=mock_cb_write, .io_read=mock_cb_read,
        .io_setdir=mock_cb_setdir, .delay_us=mock_cb_delay });

    assert(mock_cs == 1);
    elib_simbus_spi_cs_low(&ctx);
    assert(mock_cs == 0);
    elib_simbus_spi_cs_high(&ctx);
    assert(mock_cs == 1);
}

/* ------------------------------------------------------------------ */
/*  Main                                                                */
/* ------------------------------------------------------------------ */

int main(void)
{
    printf("=== elib_simbus SPI Tests ===\n");

    printf("\n-- Init / Deinit --\n");
    RUN_TEST(test_init_valid);
    RUN_TEST(test_init_null_ctx);
    RUN_TEST(test_init_null_cfg);
    RUN_TEST(test_init_null_callbacks);
    RUN_TEST(test_init_invalid_mode);
    RUN_TEST(test_deinit);

    printf("\n-- Parameter Validation --\n");
    RUN_TEST(test_transfer_null_ctx);
    RUN_TEST(test_transfer_not_init);
    RUN_TEST(test_transfer_exceed_max);
    RUN_TEST(test_transfer_zero_len);

    printf("\n-- Functional --\n");
    RUN_TEST(test_mode0_transfer);
    RUN_TEST(test_mode0_write);
    RUN_TEST(test_mode0_read);
    RUN_TEST(test_cs_helpers);

    printf("\n=== ALL PASS ===\n");
    return 0;
}
