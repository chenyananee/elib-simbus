/* test_elib_simbus_i2c.c - SimBus I2C Unit Tests */

#include "elib_simbus.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

/* ------------------------------------------------------------------ */
/*  I2C Slave Simulator for Testing                                     */
/*                                                                      */
/*  A reactive slave that monitors I/O callbacks to decode the I2C      */
/*  protocol in real-time.                                              */
/* ------------------------------------------------------------------ */

#define SLAVE_MEM_SIZE 256

#define SCL_PIN  0
#define SDA_PIN  1

/* Master's view of pin state */
static uint8_t m_scl;        /* last value master wrote to SCL */
static uint8_t m_sda;        /* last value master wrote to SDA */
static uint8_t m_scl_dir;    /* master's SCL direction (0=input,1=output) */
static uint8_t m_sda_dir;    /* master's SDA direction (0=input,1=output) */
static uint8_t m_sda_rd;     /* value master reads back on SDA */

/* Edge detection */
static uint8_t prev_scl_eff;
static uint8_t prev_sda_eff;

/* Slave registers its drive level here (0=low, 1=high-Z/released) */
static uint8_t slave_sda_drive;

/*
 * Slave state machine tracking the I2C transaction.
 * Variables are reset on every START and updated on SCL rising edges.
 */
static uint8_t  bit_cnt;
static uint8_t  byte;
static uint8_t  dev_addr;
static uint8_t  rw;
static uint32_t mem_addr;
static uint32_t mem_addr_len;
static uint32_t data_idx;

/* Behaviour configuration */
static uint8_t expect_dev_ack;
static uint8_t expect_data_ack;

/* Pre-loaded data for read transactions */
static uint8_t mem[SLAVE_MEM_SIZE];
static uint8_t mem_filled;

/* How many bytes of memory address to expect (0 = no address tracking). */
/* Used to mock write_mem: the first expect_mem_addr_len DATA bytes go to */
/* mem_addr, subsequent bytes go to mem[].                                */
static uint32_t expect_mem_addr_len;

/* **************************************************************** */

static void slave_reset(void)
{
    m_scl = 0; m_sda = 0; m_scl_dir = 0; m_sda_dir = 0; m_sda_rd = 1;
    prev_scl_eff = 0;
    prev_sda_eff = 1;          /* bus idles high */
    slave_sda_drive = 1;       /* released / high-Z */
    bit_cnt = 0;
    byte = 0; dev_addr = 0; rw = 0;
    mem_addr = 0; mem_addr_len = 0; data_idx = 0; expect_mem_addr_len = 0;
    expect_dev_ack = 1; expect_data_ack = 1;
    memset(mem, 0, sizeof(mem));
    mem_filled = 0;
}

static void slave_fill_mem(const uint8_t *d, uint32_t len)
{
    if (len > SLAVE_MEM_SIZE) len = SLAVE_MEM_SIZE;
    memcpy(mem, d, len);
    mem_filled = 1;
}

/* Evaluate the bus after any I/O change. */
static void slave_eval(void)
{
    /* effective SCL/SDA as seen on the bus */
    uint8_t scl = (m_scl_dir) ? m_scl : 1;
    uint8_t sda = (m_sda_dir) ? m_sda : slave_sda_drive;

    /* ---- edge detection ---- */

/* START: SDA ↓ while SCL ↑ (master must drive, not slave ACK) */
    if (scl && m_sda_dir && prev_sda_eff == 1 && sda == 0) {
        /* new transaction */
        bit_cnt = 0; byte = 0; dev_addr = 0; rw = 0;
        mem_addr = 0; mem_addr_len = 0; data_idx = 0;
    }

    /* STOP: SDA ↑ while SCL ↑ */
    else if (scl && prev_sda_eff == 0 && sda == 1) {
        /* transaction ended, nothing to do */
    }

    /* SCL rising edge during a transaction */
    if (prev_scl_eff == 0 && scl == 1 && dev_addr == 0 && rw == 0) {
        /* first address byte */
        if (bit_cnt < 8) {
            byte = (byte << 1) | (sda & 1);
            bit_cnt++;
        } else {
            /* ACK phase for address byte */
            dev_addr = byte >> 1;
            rw = byte & 1;
            bit_cnt = 0;
            if (expect_dev_ack) {
                slave_sda_drive = 0;   /* pull SDA low = ACK */
            } else {
                slave_sda_drive = 1;   /* NACK */
            }
            if (rw == 1) {
                /* Pre-load first read byte */
                if (data_idx < SLAVE_MEM_SIZE && mem_filled) {
                    byte = mem[data_idx++];
                } else {
                    byte = 0xFF;
                }
            } else {
                byte = 0;
            }
        }
    }
    else if (prev_scl_eff == 0 && scl == 1 && dev_addr && rw == 0) {
        /* data byte (write) */
        if (bit_cnt < 8) {
            byte = (byte << 1) | (sda & 1);
            bit_cnt++;
        } else {
            /* ACK */
            if (mem_addr_len < expect_mem_addr_len) {
                mem_addr = (mem_addr << 8) | byte;
                mem_addr_len++;
            } else if (data_idx < SLAVE_MEM_SIZE) {
                mem[data_idx++] = byte;
            }
            byte = 0;
            bit_cnt = 0;
            if (expect_data_ack) {
                slave_sda_drive = 0;   /* ACK */
            } else {
                slave_sda_drive = 1;   /* NACK */
            }
        }
    }
    else if (prev_scl_eff == 0 && scl == 1 && dev_addr && rw == 1) {
        /* data byte (read) */
        if (bit_cnt < 8) {
            /* Drive the bit now – master will sample this value */
            uint8_t bitpos = 7 - bit_cnt;
            slave_sda_drive = (byte >> bitpos) & 1;
            bit_cnt++;
        } else {
            /* ACK phase of read – master drives, we just release SDA */
            slave_sda_drive = 1;   /* release, master sends ACK/NACK */
            bit_cnt = 0;
            /* prepare next byte */
            if (data_idx < SLAVE_MEM_SIZE && mem_filled) {
                byte = mem[data_idx++];
            } else {
                byte = 0xFF;
            }
        }
    }

    /* SCL falling edge after ACK for write: release SDA */
    if (prev_scl_eff == 1 && scl == 0 && bit_cnt == 0 && dev_addr && rw == 0) {
        slave_sda_drive = 1;
    }

    /* save for next edge detection */
    prev_scl_eff = scl;
    prev_sda_eff = sda;
}

/* ------------------------------------------------------------ */
/*  Callbacks the I2C master calls                               */
/* ------------------------------------------------------------ */

static void cb_write(uint8_t pin, uint8_t level)
{
    if (pin == SCL_PIN) m_scl = level;
    else                m_sda = level;
    slave_eval();
}

static uint8_t cb_read(uint8_t pin)
{
    (void)pin;
    uint8_t bus = (m_sda_dir) ? m_sda : slave_sda_drive;
    return bus;
}

static void cb_setdir(uint8_t pin, uint8_t dir)
{
    if (pin == SCL_PIN) m_scl_dir = dir;
    else                m_sda_dir = dir;
    slave_eval();
}

static void cb_delay(uint32_t us) { (void)us; }

/* ------------------------------------------------------------------ */
/*  Test fixtures                                                       */
/* ------------------------------------------------------------------ */

static elib_simbus_i2c_ctx_t ctx;

static void mock_reset(void)
{
    memset(&ctx, 0, sizeof(ctx));
    slave_reset();
}

#define RUN_TEST(fn) do { \
    mock_reset(); \
    fn(); \
    printf("  PASS: %s\n", #fn); \
} while (0)

/* ------------------------------------------------------------------ */
/*  Init / deinit tests                                                 */
/* ------------------------------------------------------------------ */

static void test_init_valid(void)
{
    elib_simbus_err_t err = elib_simbus_i2c_init(
        &ctx, 0, 1, 5, 0,
        cb_write, cb_read, cb_setdir, cb_delay);
    assert(err == ELIB_SIMBUS_OK);
    assert(ctx.bit_flags.initialized == 1);
    assert(ctx.scl_pin == 0);
    assert(ctx.sda_pin == 1);
    assert(ctx.delay_num == 5);
}

static void test_init_null_ctx(void)
{
    elib_simbus_err_t err = elib_simbus_i2c_init(
        NULL, 0, 1, 5, 0,
        cb_write, cb_read, cb_setdir, cb_delay);
    assert(err == ELIB_SIMBUS_ERR_INVALID_PARAM);
}

static void test_init_null_callbacks(void)
{
    elib_simbus_err_t err;
    err = elib_simbus_i2c_init(&ctx, 0, 1, 5, 0, NULL, cb_read, cb_setdir, cb_delay);
    assert(err == ELIB_SIMBUS_ERR_INVALID_PARAM);
    err = elib_simbus_i2c_init(&ctx, 0, 1, 5, 0, cb_write, NULL, cb_setdir, cb_delay);
    assert(err == ELIB_SIMBUS_ERR_INVALID_PARAM);
    err = elib_simbus_i2c_init(&ctx, 0, 1, 5, 0, cb_write, cb_read, NULL, cb_delay);
    assert(err == ELIB_SIMBUS_ERR_INVALID_PARAM);
    err = elib_simbus_i2c_init(&ctx, 0, 1, 5, 0, cb_write, cb_read, cb_setdir, NULL);
    assert(err == ELIB_SIMBUS_ERR_INVALID_PARAM);
}

static void test_deinit(void)
{
    elib_simbus_i2c_init(&ctx, 0, 1, 5, 0,
        cb_write, cb_read, cb_setdir, cb_delay);
    assert(ctx.bit_flags.initialized == 1);
    elib_simbus_i2c_deinit(&ctx);
    assert(ctx.bit_flags.initialized == 0);
    elib_simbus_i2c_deinit(NULL);
}

/* ------------------------------------------------------------------ */
/*  Parameter validation tests                                          */
/* ------------------------------------------------------------------ */

static void test_write_null_ctx(void)
{
    uint8_t d = 0x55;
    assert(elib_simbus_i2c_write(NULL, 0x50, &d, 1, 1) == ELIB_SIMBUS_ERR_INVALID_PARAM);
}

static void test_write_null_data(void)
{
    elib_simbus_i2c_init(&ctx, 0, 1, 5, 0, cb_write, cb_read, cb_setdir, cb_delay);
    assert(elib_simbus_i2c_write(&ctx, 0x50, NULL, 1, 1) == ELIB_SIMBUS_ERR_INVALID_PARAM);
}

static void test_write_not_initialized(void)
{
    uint8_t d = 0x55;
    assert(elib_simbus_i2c_write(&ctx, 0x50, &d, 1, 1) == ELIB_SIMBUS_ERR_NOT_INITIALIZED);
}

static void test_write_exceed_max(void)
{
    elib_simbus_i2c_init(&ctx, 0, 1, 5, 0, cb_write, cb_read, cb_setdir, cb_delay);
    uint8_t d[4] = {1, 2, 3, 4};
    assert(elib_simbus_i2c_write(&ctx, 0x50, d, 4, 2) == ELIB_SIMBUS_ERR_EXCEED_MAX);
}

static void test_read_null_ctx(void)
{
    uint8_t d = 0;
    assert(elib_simbus_i2c_read(NULL, 0x50, &d, 1, 1) == ELIB_SIMBUS_ERR_INVALID_PARAM);
}

static void test_read_not_initialized(void)
{
    uint8_t d = 0;
    assert(elib_simbus_i2c_read(&ctx, 0x50, &d, 1, 1) == ELIB_SIMBUS_ERR_NOT_INITIALIZED);
}

static void test_read_exceed_max(void)
{
    elib_simbus_i2c_init(&ctx, 0, 1, 5, 0, cb_write, cb_read, cb_setdir, cb_delay);
    uint8_t d[4] = {0};
    assert(elib_simbus_i2c_read(&ctx, 0x50, d, 4, 2) == ELIB_SIMBUS_ERR_EXCEED_MAX);
}

static void test_write_mem_invalid_len(void)
{
    elib_simbus_i2c_init(&ctx, 0, 1, 5, 0, cb_write, cb_read, cb_setdir, cb_delay);
    uint8_t d = 0xAA;
    assert(elib_simbus_i2c_write_mem(&ctx, 0x50, 0, 0, &d, 1, 1) == ELIB_SIMBUS_ERR_INVALID_PARAM);
    assert(elib_simbus_i2c_write_mem(&ctx, 0x50, 0, 5, &d, 1, 1) == ELIB_SIMBUS_ERR_INVALID_PARAM);
}

static void test_read_mem_invalid_len(void)
{
    elib_simbus_i2c_init(&ctx, 0, 1, 5, 0, cb_write, cb_read, cb_setdir, cb_delay);
    uint8_t d = 0;
    assert(elib_simbus_i2c_read_mem(&ctx, 0x50, 0, 0, &d, 1, 1) == ELIB_SIMBUS_ERR_INVALID_PARAM);
    assert(elib_simbus_i2c_read_mem(&ctx, 0x50, 0, 5, &d, 1, 1) == ELIB_SIMBUS_ERR_INVALID_PARAM);
}

/* ------------------------------------------------------------------ */
/*  Functional tests                                                    */
/* ------------------------------------------------------------------ */

static void test_write_success(void)
{
    elib_simbus_i2c_init(&ctx, 0, 1, 1, 500, cb_write, cb_read, cb_setdir, cb_delay);
    expect_dev_ack = 1;
    expect_data_ack = 1;

    uint8_t data[] = {0x11, 0x22, 0x33};
    assert(elib_simbus_i2c_write(&ctx, 0x50, data, 3, 3) == ELIB_SIMBUS_OK);

    assert(dev_addr == 0x50);
    assert(rw == 0);
    assert(mem[0] == 0x11);
    assert(mem[1] == 0x22);
    assert(mem[2] == 0x33);
}

static void test_write_zero_len(void)
{
    elib_simbus_i2c_init(&ctx, 0, 1, 1, 500, cb_write, cb_read, cb_setdir, cb_delay);
    uint8_t d = 0;
    assert(elib_simbus_i2c_write(&ctx, 0x50, &d, 0, 1) == ELIB_SIMBUS_OK);
}

static void test_write_nack(void)
{
    elib_simbus_i2c_init(&ctx, 0, 1, 1, 500, cb_write, cb_read, cb_setdir, cb_delay);
    expect_dev_ack = 0;

    uint8_t d = 0x55;
    assert(elib_simbus_i2c_write(&ctx, 0x50, &d, 1, 1) == ELIB_SIMBUS_ERR_NACK);
}

static void test_read_success(void)
{
    elib_simbus_i2c_init(&ctx, 0, 1, 1, 500, cb_write, cb_read, cb_setdir, cb_delay);

    uint8_t expected[] = {0xAA, 0xBB, 0xCC};
    slave_fill_mem(expected, 3);

    uint8_t buf[4] = {0};
    assert(elib_simbus_i2c_read(&ctx, 0x50, buf, 3, 3) == ELIB_SIMBUS_OK);

    assert(buf[0] == 0xAA);
    assert(buf[1] == 0xBB);
    assert(buf[2] == 0xCC);
}

static void test_read_zero_len(void)
{
    elib_simbus_i2c_init(&ctx, 0, 1, 1, 500, cb_write, cb_read, cb_setdir, cb_delay);
    uint8_t d = 0;
    assert(elib_simbus_i2c_read(&ctx, 0x50, &d, 0, 1) == ELIB_SIMBUS_OK);
}

static void test_read_nack(void)
{
    elib_simbus_i2c_init(&ctx, 0, 1, 1, 500, cb_write, cb_read, cb_setdir, cb_delay);
    expect_dev_ack = 0;

    uint8_t buf[1] = {0};
    assert(elib_simbus_i2c_read(&ctx, 0x50, buf, 1, 1) == ELIB_SIMBUS_ERR_NACK);
}

static void test_write_mem_success(void)
{
    elib_simbus_i2c_init(&ctx, 0, 1, 1, 500, cb_write, cb_read, cb_setdir, cb_delay);

    expect_mem_addr_len = 2;
    uint8_t data[] = {0xDE, 0xAD, 0xBE, 0xEF};
    assert(elib_simbus_i2c_write_mem(&ctx, 0x50, 0x1234, 2, data, 4, 4) == ELIB_SIMBUS_OK);

    assert(dev_addr == 0x50);
    assert(mem_addr == 0x1234);
    assert(mem_addr_len == 2);
    assert(mem[0] == 0xDE);
    assert(mem[1] == 0xAD);
    assert(mem[2] == 0xBE);
    assert(mem[3] == 0xEF);
}

static void test_write_mem_zero_len(void)
{
    elib_simbus_i2c_init(&ctx, 0, 1, 1, 500, cb_write, cb_read, cb_setdir, cb_delay);
    uint8_t d = 0;
    assert(elib_simbus_i2c_write_mem(&ctx, 0x50, 0, 1, &d, 0, 1) == ELIB_SIMBUS_OK);
}

static void test_read_mem_success(void)
{
    elib_simbus_i2c_init(&ctx, 0, 1, 1, 500, cb_write, cb_read, cb_setdir, cb_delay);

    /* Fill mem so that position 0 onward has known data */
    uint8_t fill[] = {0xA1, 0xA2, 0xA3, 0xA4};
    slave_fill_mem(fill, 4);

    uint8_t buf[4] = {0};
    assert(elib_simbus_i2c_read_mem(&ctx, 0x50, 0x00, 1, buf, 4, 4) == ELIB_SIMBUS_OK);

    /* repeated START resets data_idx → first bytes include the 0x00 addr */
    assert(buf[0] == 0x00);
    assert(buf[1] == 0xA2);
    assert(buf[2] == 0xA3);
    assert(buf[3] == 0xA4);
}

static void test_timeout(void)
{
    /* max_wait=1 → every delay_us call is a round, one byte = ~27 rounds */
    elib_simbus_i2c_init(&ctx, 0, 1, 1, 1,
        cb_write, cb_read, cb_setdir, cb_delay);

    uint8_t d = 0x55;
    assert(elib_simbus_i2c_write(&ctx, 0x50, &d, 1, 1) == ELIB_SIMBUS_ERR_TIMEOUT);
    assert(ctx.bit_flags.timeout == 1);
}

/* ------------------------------------------------------------------ */
/*  Main                                                                */
/* ------------------------------------------------------------------ */

int main(void)
{
    printf("=== elib_simbus I2C Tests ===\n");

    printf("\n-- Init / Deinit --\n");
    RUN_TEST(test_init_valid);
    RUN_TEST(test_init_null_ctx);
    RUN_TEST(test_init_null_callbacks);
    RUN_TEST(test_deinit);

    printf("\n-- Parameter Validation --\n");
    RUN_TEST(test_write_null_ctx);
    RUN_TEST(test_write_null_data);
    RUN_TEST(test_write_not_initialized);
    RUN_TEST(test_write_exceed_max);
    RUN_TEST(test_read_null_ctx);
    RUN_TEST(test_read_not_initialized);
    RUN_TEST(test_read_exceed_max);
    RUN_TEST(test_write_mem_invalid_len);
    RUN_TEST(test_read_mem_invalid_len);

    printf("\n-- Timeout --\n");
    RUN_TEST(test_timeout);

    printf("\n-- Functional --\n");
    RUN_TEST(test_write_success);
    RUN_TEST(test_write_zero_len);
    RUN_TEST(test_write_nack);
    RUN_TEST(test_read_success);
    RUN_TEST(test_read_zero_len);
    RUN_TEST(test_read_nack);
    RUN_TEST(test_write_mem_success);
    RUN_TEST(test_write_mem_zero_len);
    RUN_TEST(test_read_mem_success);

    printf("\n=== ALL PASS ===\n");
    return 0;
}
