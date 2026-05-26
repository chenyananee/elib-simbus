/* test_elib_simbus_ow.c - SimBus 1-Wire Unit Tests */

#include "elib_simbus.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

#define PIN_DQ  0

static uint8_t mock_dq_val;
static uint8_t mock_dq_dir;
static uint32_t mock_delay_cnt;
static uint8_t slave_driving_low;

static void mock_reset_all(void)
{
    mock_dq_val = 1;
    mock_dq_dir = 0;
    mock_delay_cnt = 0;
    slave_driving_low = 0;
}

static void cb_w(uint8_t pin, uint8_t lvl) { (void)pin; mock_dq_val = lvl; }
static uint8_t cb_r(uint8_t pin) {
    (void)pin;
    if (slave_driving_low) return 0;
    return mock_dq_dir ? mock_dq_val : 1;
}
static void cb_d(uint8_t pin, uint8_t dir) { (void)pin; mock_dq_dir = dir; }
static void cb_delay(uint32_t us) { (void)us; mock_delay_cnt++; }

static uint8_t slave_present;
static uint32_t slave_pull_at;

static void cb_delay_slave(uint32_t us)
{
    (void)us;
    mock_delay_cnt++;
    if (mock_delay_cnt == slave_pull_at && slave_present) {
        slave_driving_low = 1;
    }
}

static elib_simbus_ow_ctx_t ctx;

static void reset(void)
{
    memset(&ctx, 0, sizeof(ctx));
    mock_reset_all();
    slave_present = 0;
}

#define RUN_TEST(fn) do { reset(); fn(); printf("  PASS: %s\n", #fn); } while (0)

static void init_valid(void) {
    elib_simbus_err_t e = elib_simbus_ow_init(&ctx, &(elib_simbus_ow_cfg_t){
        .dq_pin=PIN_DQ,.delay_num=1,
        .io_write=cb_w,.io_read=cb_r,.io_setdir=cb_d,.delay_us=cb_delay });
    assert(e == ELIB_SIMBUS_OK); assert(ctx.bit_flags.initialized == 1);
}

static void init_null_ctx(void) {
    assert(elib_simbus_ow_init(NULL, &(elib_simbus_ow_cfg_t){
        .dq_pin=0,.delay_num=1,.io_write=cb_w,.io_read=cb_r,
        .io_setdir=cb_d,.delay_us=cb_delay }) == ELIB_SIMBUS_ERR_INVALID_PARAM);
}

static void init_null_cfg(void) {
    assert(elib_simbus_ow_init(&ctx, NULL) == ELIB_SIMBUS_ERR_INVALID_PARAM);
}

static void init_null_cbs(void) {
    elib_simbus_err_t e;
    e = elib_simbus_ow_init(&ctx, &(elib_simbus_ow_cfg_t){
        .dq_pin=0,.delay_num=1,.io_write=NULL,.io_read=cb_r,
        .io_setdir=cb_d,.delay_us=cb_delay });
    assert(e == ELIB_SIMBUS_ERR_INVALID_PARAM);
    e = elib_simbus_ow_init(&ctx, &(elib_simbus_ow_cfg_t){
        .dq_pin=0,.delay_num=1,.io_write=cb_w,.io_read=NULL,
        .io_setdir=cb_d,.delay_us=cb_delay });
    assert(e == ELIB_SIMBUS_ERR_INVALID_PARAM);
}

static void init_delay_zero(void) {
    assert(elib_simbus_ow_init(&ctx, &(elib_simbus_ow_cfg_t){
        .dq_pin=0,.delay_num=0,.io_write=cb_w,.io_read=cb_r,
        .io_setdir=cb_d,.delay_us=cb_delay }) == ELIB_SIMBUS_ERR_INVALID_PARAM);
}

static void deinit(void) {
    elib_simbus_ow_init(&ctx, &(elib_simbus_ow_cfg_t){
        .dq_pin=0,.delay_num=1,.io_write=cb_w,.io_read=cb_r,
        .io_setdir=cb_d,.delay_us=cb_delay });
    assert(ctx.bit_flags.initialized == 1);
    elib_simbus_ow_deinit(&ctx);
    assert(ctx.bit_flags.initialized == 0);
    elib_simbus_ow_deinit(NULL);
}

static void reset_present(void) {
    elib_simbus_ow_init(&ctx, &(elib_simbus_ow_cfg_t){
        .dq_pin=PIN_DQ,.delay_num=1,.timeout_rounds=100,
        .io_write=cb_w,.io_read=cb_r,.io_setdir=cb_d,.delay_us=cb_delay_slave });
    slave_present = 1; slave_pull_at = 5;  /* pull low after 3 poll rounds */
    assert(elib_simbus_ow_reset(&ctx) == 1);
}

static void reset_no_dev(void) {
    elib_simbus_ow_init(&ctx, &(elib_simbus_ow_cfg_t){
        .dq_pin=PIN_DQ,.delay_num=1,.timeout_rounds=5,
        .io_write=cb_w,.io_read=cb_r,.io_setdir=cb_d,.delay_us=cb_delay });
    assert(elib_simbus_ow_reset(&ctx) == -1);
    assert(ctx.bit_flags.timeout == 1);
}

static void reset_null(void) { assert(elib_simbus_ow_reset(NULL) == -1); }

static void write_bit1(void) {
    elib_simbus_ow_init(&ctx, &(elib_simbus_ow_cfg_t){
        .dq_pin=PIN_DQ,.delay_num=1,
        .io_write=cb_w,.io_read=cb_r,.io_setdir=cb_d,.delay_us=cb_delay });
    elib_simbus_ow_write_bit(&ctx, 1);
    assert(mock_dq_dir == 0);
}

static void write_bit0(void) {
    elib_simbus_ow_init(&ctx, &(elib_simbus_ow_cfg_t){
        .dq_pin=PIN_DQ,.delay_num=1,
        .io_write=cb_w,.io_read=cb_r,.io_setdir=cb_d,.delay_us=cb_delay });
    elib_simbus_ow_write_bit(&ctx, 0);
    assert(mock_dq_dir == 0);
}

static void write_bytes(void) {
    elib_simbus_ow_init(&ctx, &(elib_simbus_ow_cfg_t){
        .dq_pin=PIN_DQ,.delay_num=1,
        .io_write=cb_w,.io_read=cb_r,.io_setdir=cb_d,.delay_us=cb_delay });
    uint8_t d[] = {0x12,0x34};
    assert(elib_simbus_ow_write(&ctx, d, 2, 2) == ELIB_SIMBUS_OK);
    assert(mock_dq_dir == 0);
}

static void write_exceed(void) {
    elib_simbus_ow_init(&ctx, &(elib_simbus_ow_cfg_t){
        .dq_pin=PIN_DQ,.delay_num=1,
        .io_write=cb_w,.io_read=cb_r,.io_setdir=cb_d,.delay_us=cb_delay });
    uint8_t d[4] = {0};
    assert(elib_simbus_ow_write(&ctx, d, 4, 2) == ELIB_SIMBUS_ERR_EXCEED_MAX);
}

static void read_bit_low(void) {
    elib_simbus_ow_init(&ctx, &(elib_simbus_ow_cfg_t){
        .dq_pin=PIN_DQ,.delay_num=1,
        .io_write=cb_w,.io_read=cb_r,.io_setdir=cb_d,.delay_us=cb_delay });
    slave_driving_low = 1;
    assert(elib_simbus_ow_read_bit(&ctx) == 0);
}

static void read_bit_high(void) {
    elib_simbus_ow_init(&ctx, &(elib_simbus_ow_cfg_t){
        .dq_pin=PIN_DQ,.delay_num=1,
        .io_write=cb_w,.io_read=cb_r,.io_setdir=cb_d,.delay_us=cb_delay });
    slave_driving_low = 0;
    assert(elib_simbus_ow_read_bit(&ctx) == 1);
}

static void read_null(void) {
    assert(elib_simbus_ow_read(&ctx, NULL, 1, 1) == ELIB_SIMBUS_ERR_INVALID_PARAM);
}

int main(void) {
    printf("=== elib_simbus 1-Wire Tests ===\n");

    printf("\n-- Init --\n");
    RUN_TEST(init_valid); RUN_TEST(init_null_ctx); RUN_TEST(init_null_cfg);
    RUN_TEST(init_null_cbs); RUN_TEST(init_delay_zero); RUN_TEST(deinit);

    printf("\n-- Reset --\n");
    RUN_TEST(reset_present); RUN_TEST(reset_no_dev); RUN_TEST(reset_null);

    printf("\n-- Write --\n");
    RUN_TEST(write_bit1); RUN_TEST(write_bit0); RUN_TEST(write_bytes);
    RUN_TEST(write_exceed);

    printf("\n-- Read --\n");
    RUN_TEST(read_bit_low); RUN_TEST(read_bit_high); RUN_TEST(read_null);

    printf("\n=== ALL PASS ===\n");
    return 0;
}
