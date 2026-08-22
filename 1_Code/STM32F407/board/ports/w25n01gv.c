/*
 * SPDX-License-Identifier: MIT
 * Origin: copied from build/f4usb25n01 test project
 * Created-By: gpt-5.5
 *
 * 署名: czstara12
 */

#include "w25n01gv.h"

#define W25N01GV_STATUS_ECC_MASK        0x30U

#ifndef W25N01GV_UNIT_TEST
#include <rtconfig.h>
#endif

#if defined(BSP_USING_SPI_FLASH)

#include <rtdevice.h>
#include <rtthread.h>
#include <stdlib.h>
#include <string.h>

#include <drivers/mtd_nand.h>

#include <drv_gpio.h>
#include <drv_spi.h>

#define DBG_TAG "w25n01gv"
#define DBG_LVL DBG_ERROR
#include <rtdbg.h>

#define W25N01GV_SPI_BUS_NAME           "spi1"
#define W25N01GV_SPI_DEVICE_NAME        "w25ngv0"
#define W25N01GV_SPI_MAX_HZ             104000000U
#define W25N01GV_CMD_RESET              0xFFU
#define W25N01GV_CMD_READ_ID            0x9FU
#define W25N01GV_CMD_READ_FEATURE       0x0FU
#define W25N01GV_CMD_SET_FEATURE        0x1FU
#define W25N01GV_CMD_WRITE_ENABLE       0x06U
#define W25N01GV_CMD_PAGE_DATA_READ     0x13U
#define W25N01GV_CMD_READ_CACHE         0x03U
#define W25N01GV_CMD_PROGRAM_LOAD       0x02U
#define W25N01GV_CMD_RANDOM_PROGRAM_LOAD 0x84U
#define W25N01GV_CMD_PROGRAM_EXECUTE    0x10U
#define W25N01GV_CMD_BLOCK_ERASE        0xD8U
#define W25N01GV_REG_BLOCK_LOCK         0xA0U
#define W25N01GV_REG_CONFIG             0xB0U
#define W25N01GV_REG_STATUS             0xC0U
#define W25N01GV_CONFIG_BUF             0x08U
#define W25N01GV_CONFIG_ECC_ENABLE      0x10U
#define W25N01GV_STATUS_BUSY            0x01U
#define W25N01GV_STATUS_WEL             0x02U
#define W25N01GV_STATUS_ERASE_FAIL      0x04U
#define W25N01GV_STATUS_PROGRAM_FAIL    0x08U
#define W25N01GV_BAD_BLOCK_MARKER_COL   W25N01GV_PAGE_SIZE
#define W25N01GV_WAIT_READY_TIMEOUT_MS  1000U
#define W25N01GV_WAIT_READY_POLL_US     50U
#define W25N01GV_TEST_BLOCK_FIRST       1000U
#define W25N01GV_READ_DUMP_MAX          256U
static struct rt_spi_device *w25n01gv_spi_dev;
static uint8_t w25n01gv_page_buf[W25N01GV_PAGE_SIZE];
static struct rt_mtd_nand_device w25n01gv_mtd_dev;
static struct w25n01gv_io_stats w25n01gv_stats;

void w25n01gv_stats_reset(void)
{
    memset(&w25n01gv_stats, 0, sizeof(w25n01gv_stats));
}

void w25n01gv_stats_get(struct w25n01gv_io_stats *stats)
{
    if (stats != RT_NULL)
    {
        *stats = w25n01gv_stats;
    }
}

static rt_err_t w25n01gv_read_feature(uint8_t reg, uint8_t *value)
{
    uint8_t cmd[2] = {W25N01GV_CMD_READ_FEATURE, reg};

    if (w25n01gv_spi_dev == RT_NULL || value == RT_NULL)
    {
        return -RT_ERROR;
    }

    return rt_spi_send_then_recv(w25n01gv_spi_dev, cmd, sizeof(cmd), value, 1);
}

static rt_err_t w25n01gv_set_feature(uint8_t reg, uint8_t value)
{
    uint8_t cmd[3] = {W25N01GV_CMD_SET_FEATURE, reg, value};

    if (w25n01gv_spi_dev == RT_NULL)
    {
        return -RT_ERROR;
    }

    if (rt_spi_send(w25n01gv_spi_dev, cmd, sizeof(cmd)) != (rt_ssize_t)sizeof(cmd))
    {
        return -RT_ERROR;
    }

    return RT_EOK;
}

static rt_err_t w25n01gv_wait_ready(void)
{
    uint8_t status = 0;
    rt_tick_t start_tick = rt_tick_get();
    rt_tick_t timeout_ticks = rt_tick_from_millisecond(W25N01GV_WAIT_READY_TIMEOUT_MS);

    while ((rt_tick_get() - start_tick) < timeout_ticks)
    {
        if (w25n01gv_read_feature(W25N01GV_REG_STATUS, &status) != RT_EOK)
        {
            return -RT_ERROR;
        }

        if ((status & W25N01GV_STATUS_BUSY) == 0U)
        {
            return RT_EOK;
        }

        w25n01gv_stats.busy_wait_us += W25N01GV_WAIT_READY_POLL_US;
        /* ponytail: 短轮询优先保证 NAND 延迟；若块擦除影响调度响应，再为长等待加入让步。 */
        rt_hw_us_delay(W25N01GV_WAIT_READY_POLL_US);
    }

    return -RT_ETIMEOUT;
}

static rt_err_t w25n01gv_reset(void)
{
    uint8_t cmd = W25N01GV_CMD_RESET;

    if (rt_spi_send(w25n01gv_spi_dev, &cmd, 1) != 1)
    {
        return -RT_ERROR;
    }

    rt_thread_mdelay(1);
    return w25n01gv_wait_ready();
}

static rt_err_t w25n01gv_write_enable(void)
{
    uint8_t cmd = W25N01GV_CMD_WRITE_ENABLE;

    if (w25n01gv_spi_dev == RT_NULL)
    {
        return -RT_ERROR;
    }

    if (rt_spi_send(w25n01gv_spi_dev, &cmd, 1) != 1)
    {
        return -RT_ERROR;
    }

    return RT_EOK;
}

static rt_err_t w25n01gv_unlock_all_blocks(void)
{
    uint8_t lock = 0;
    rt_err_t ret;

    ret = w25n01gv_set_feature(W25N01GV_REG_BLOCK_LOCK, 0x00);
    if (ret != RT_EOK)
    {
        return ret;
    }

    ret = w25n01gv_read_feature(W25N01GV_REG_BLOCK_LOCK, &lock);
    if (ret != RT_EOK)
    {
        return ret;
    }

    return lock == 0x00U ? RT_EOK : -RT_ERROR;
}

static rt_err_t w25n01gv_configure_for_raw_oob(void)
{
    uint8_t config = 0;
    rt_err_t ret;

    ret = w25n01gv_read_feature(W25N01GV_REG_CONFIG, &config);
    if (ret != RT_EOK)
    {
        return ret;
    }

    config |= W25N01GV_CONFIG_BUF;
    config &= (uint8_t)~W25N01GV_CONFIG_ECC_ENABLE;

    ret = w25n01gv_set_feature(W25N01GV_REG_CONFIG, config);
    if (ret != RT_EOK)
    {
        return ret;
    }

    ret = w25n01gv_read_feature(W25N01GV_REG_CONFIG, &config);
    if (ret != RT_EOK)
    {
        return ret;
    }

    return ((config & W25N01GV_CONFIG_BUF) != 0U &&
            (config & W25N01GV_CONFIG_ECC_ENABLE) == 0U) ? RT_EOK : -RT_ERROR;
}

static rt_err_t w25n01gv_read_jedec_id(uint8_t id[W25N01GV_JEDEC_ID_LEN])
{
    uint8_t cmd[2] = {W25N01GV_CMD_READ_ID, 0x00};

    if (w25n01gv_spi_dev == RT_NULL || id == RT_NULL)
    {
        return -RT_ERROR;
    }

    return rt_spi_send_then_recv(w25n01gv_spi_dev, cmd, sizeof(cmd), id, W25N01GV_JEDEC_ID_LEN);
}

static rt_err_t w25n01gv_read_cache(uint16_t column, uint8_t *buf, rt_size_t len)
{
    uint8_t cmd[4];

    if (w25n01gv_spi_dev == RT_NULL || buf == RT_NULL || len == 0U)
    {
        return -RT_ERROR;
    }

    cmd[0] = W25N01GV_CMD_READ_CACHE;
    cmd[1] = (uint8_t)(column >> 8);
    cmd[2] = (uint8_t)column;
    cmd[3] = 0x00;

    w25n01gv_stats.cache_read_bytes += len;
    return rt_spi_send_then_recv(w25n01gv_spi_dev, cmd, sizeof(cmd), buf, len);
}

static rt_err_t w25n01gv_program_load_cmd(uint8_t command, uint16_t column, const uint8_t *buf, rt_size_t len)
{
    uint8_t cmd[3];

    if (w25n01gv_spi_dev == RT_NULL || buf == RT_NULL || len == 0U)
    {
        return -RT_ERROR;
    }

    cmd[0] = command;
    cmd[1] = (uint8_t)(column >> 8);
    cmd[2] = (uint8_t)column;

    w25n01gv_stats.program_load_bytes += len;
    return rt_spi_send_then_send(w25n01gv_spi_dev, cmd, sizeof(cmd), buf, len);
}

static rt_err_t w25n01gv_program_load(uint16_t column, const uint8_t *buf, rt_size_t len)
{
    return w25n01gv_program_load_cmd(W25N01GV_CMD_PROGRAM_LOAD, column, buf, len);
}

static rt_err_t w25n01gv_random_program_load(uint16_t column, const uint8_t *buf, rt_size_t len)
{
    return w25n01gv_program_load_cmd(W25N01GV_CMD_RANDOM_PROGRAM_LOAD, column, buf, len);
}

static rt_err_t w25n01gv_program_execute(uint32_t row)
{
    uint8_t cmd[4];
    uint8_t status = 0;
    rt_err_t ret;

    if (w25n01gv_spi_dev == RT_NULL)
    {
        return -RT_ERROR;
    }

    cmd[0] = W25N01GV_CMD_PROGRAM_EXECUTE;
    cmd[1] = (uint8_t)(row >> 16);
    cmd[2] = (uint8_t)(row >> 8);
    cmd[3] = (uint8_t)row;

    w25n01gv_stats.program_pages++;
    if (rt_spi_send(w25n01gv_spi_dev, cmd, sizeof(cmd)) != (rt_ssize_t)sizeof(cmd))
    {
        return -RT_ERROR;
    }

    ret = w25n01gv_wait_ready();
    if (ret != RT_EOK)
    {
        return ret;
    }

    if (w25n01gv_read_feature(W25N01GV_REG_STATUS, &status) != RT_EOK)
    {
        return -RT_ERROR;
    }

    if (status & W25N01GV_STATUS_PROGRAM_FAIL)
    {
        rt_kprintf("W25N01GV: program status=0x%02x\n", status);
        return -RT_ERROR;
    }

    return RT_EOK;
}

static rt_err_t w25n01gv_load_page(uint32_t row)
{
    uint8_t cmd[4];

    if (w25n01gv_spi_dev == RT_NULL)
    {
        return -RT_ERROR;
    }

    cmd[0] = W25N01GV_CMD_PAGE_DATA_READ;
    cmd[1] = (uint8_t)(row >> 16);
    cmd[2] = (uint8_t)(row >> 8);
    cmd[3] = (uint8_t)row;

    w25n01gv_stats.load_pages++;
    if (rt_spi_send(w25n01gv_spi_dev, cmd, sizeof(cmd)) != (rt_ssize_t)sizeof(cmd))
    {
        return -RT_ERROR;
    }

    return w25n01gv_wait_ready();
}

static rt_err_t w25n01gv_read_page(uint16_t block, uint8_t page, uint8_t *buf, rt_size_t len)
{
    rt_err_t ret;

    if (buf == RT_NULL || len == 0U || len > W25N01GV_PAGE_SIZE ||
        block >= W25N01GV_BLOCK_COUNT || page >= W25N01GV_PAGES_PER_BLOCK)
    {
        return -RT_ERROR;
    }

    ret = w25n01gv_load_page(w25n01gv_row_from_block_page(block, page));
    if (ret != RT_EOK)
    {
        return ret;
    }

    return w25n01gv_read_cache(0, buf, len);
}

static rt_err_t w25n01gv_erase_block(uint16_t block)
{
    uint8_t cmd[4];
    uint8_t status = 0;
    uint32_t row;
    rt_err_t ret;

    if (block >= W25N01GV_BLOCK_COUNT)
    {
        return -RT_ERROR;
    }

    row = w25n01gv_row_from_block_page(block, 0);
    cmd[0] = W25N01GV_CMD_BLOCK_ERASE;
    cmd[1] = (uint8_t)(row >> 16);
    cmd[2] = (uint8_t)(row >> 8);
    cmd[3] = (uint8_t)row;

    w25n01gv_stats.erase_blocks++;
    ret = w25n01gv_write_enable();
    if (ret != RT_EOK)
    {
        return ret;
    }

    if (rt_spi_send(w25n01gv_spi_dev, cmd, sizeof(cmd)) != (rt_ssize_t)sizeof(cmd))
    {
        return -RT_ERROR;
    }

    ret = w25n01gv_wait_ready();
    if (ret != RT_EOK)
    {
        return ret;
    }

    if (w25n01gv_read_feature(W25N01GV_REG_STATUS, &status) != RT_EOK)
    {
        return -RT_ERROR;
    }

    if (status & W25N01GV_STATUS_ERASE_FAIL)
    {
        rt_kprintf("W25N01GV: erase status=0x%02x\n", status);
        return -RT_ERROR;
    }

    return RT_EOK;
}

static rt_err_t w25n01gv_write_page(uint16_t block, uint8_t page, const uint8_t *buf, rt_size_t len)
{
    rt_err_t ret;

    if (buf == RT_NULL || len == 0U || len > W25N01GV_PAGE_SIZE ||
        block >= W25N01GV_BLOCK_COUNT || page >= W25N01GV_PAGES_PER_BLOCK)
    {
        return -RT_ERROR;
    }

    ret = w25n01gv_write_enable();
    if (ret != RT_EOK)
    {
        return ret;
    }

    ret = w25n01gv_program_load(0, buf, len);
    if (ret != RT_EOK)
    {
        return ret;
    }

    return w25n01gv_program_execute(w25n01gv_row_from_block_page(block, page));
}

static rt_err_t w25n01gv_read_bad_block_marker(uint16_t block, uint8_t *marker)
{
    rt_err_t ret;
    uint32_t row = w25n01gv_row_from_block_page(block, 0);

    ret = w25n01gv_load_page(row);
    if (ret != RT_EOK)
    {
        return ret;
    }

    ret = w25n01gv_read_cache(W25N01GV_BAD_BLOCK_MARKER_COL, marker, 1);
    if (ret != RT_EOK)
    {
        return ret;
    }

    return RT_EOK;
}

static rt_err_t w25n01gv_read_oob(uint16_t block, uint8_t page, uint16_t column, uint8_t *buf, rt_size_t len)
{
    rt_err_t ret;

    if (buf == RT_NULL || len == 0U || column + len > W25N01GV_OOB_SIZE)
    {
        return -RT_ERROR;
    }

    ret = w25n01gv_load_page(w25n01gv_row_from_block_page(block, page));
    if (ret != RT_EOK)
    {
        return ret;
    }

    return w25n01gv_read_cache(W25N01GV_PAGE_SIZE + column, buf, len);
}

static rt_err_t w25n01gv_write_page_with_oob(uint16_t block,
                                             uint8_t page,
                                             const uint8_t *data,
                                             rt_size_t data_len,
                                             const uint8_t *spare,
                                             rt_size_t spare_len)
{
    rt_err_t ret;
    uint8_t status = 0;

    if (block >= W25N01GV_BLOCK_COUNT || page >= W25N01GV_PAGES_PER_BLOCK ||
        (data == RT_NULL && data_len != 0U) ||
        (spare == RT_NULL && spare_len != 0U) ||
        data_len > W25N01GV_PAGE_SIZE ||
        spare_len > W25N01GV_OOB_SIZE)
    {
        return -RT_ERROR;
    }

    ret = w25n01gv_write_enable();
    if (ret != RT_EOK)
    {
        return ret;
    }

    if (w25n01gv_read_feature(W25N01GV_REG_STATUS, &status) != RT_EOK ||
        (status & W25N01GV_STATUS_WEL) == 0U)
    {
        return -RT_ERROR;
    }

    if (data_len > 0U)
    {
        ret = w25n01gv_program_load(0, data, data_len);
        if (ret != RT_EOK)
        {
            return ret;
        }
    }

    if (spare_len > 0U)
    {
        ret = data_len > 0U ?
              w25n01gv_random_program_load(W25N01GV_PAGE_SIZE, spare, spare_len) :
              w25n01gv_program_load(W25N01GV_PAGE_SIZE, spare, spare_len);
        if (ret != RT_EOK)
        {
            return ret;
        }
    }

    return w25n01gv_program_execute(w25n01gv_row_from_block_page(block, page));
}

static rt_err_t w25n01gv_mtd_read_id(struct rt_mtd_nand_device *device)
{
    uint8_t id[W25N01GV_JEDEC_ID_LEN] = {0};

    (void)device;

    return w25n01gv_read_jedec_id(id);
}

static rt_err_t w25n01gv_mtd_read_page(struct rt_mtd_nand_device *device,
                                       rt_off_t page,
                                       rt_uint8_t *data,
                                       rt_uint32_t data_len,
                                       rt_uint8_t *spare,
                                       rt_uint32_t spare_len)
{
    uint16_t block = (uint16_t)(page / W25N01GV_PAGES_PER_BLOCK);
    uint8_t page_in_block = (uint8_t)(page % W25N01GV_PAGES_PER_BLOCK);

    (void)device;

    if (block < W25N01GV_UFFS_BLOCK_FIRST ||
        block >= W25N01GV_UFFS_BLOCK_FIRST + W25N01GV_UFFS_BLOCK_COUNT ||
        (data == RT_NULL && data_len != 0U) ||
        (spare == RT_NULL && spare_len != 0U) ||
        data_len > W25N01GV_PAGE_SIZE ||
        spare_len > W25N01GV_OOB_SIZE)
    {
        return -RT_ERROR;
    }

    if ((data_len > 0U || spare_len > 0U) &&
        w25n01gv_load_page(w25n01gv_row_from_block_page(block, page_in_block)) != RT_EOK)
    {
        return -RT_ERROR;
    }

    if (data != RT_NULL && data_len > 0U)
    {
        if (w25n01gv_read_cache(0, data, data_len) != RT_EOK)
        {
            return -RT_ERROR;
        }
    }

    if (spare != RT_NULL && spare_len > 0U)
    {
        if (w25n01gv_read_cache(W25N01GV_PAGE_SIZE, spare, spare_len) != RT_EOK)
        {
            return -RT_ERROR;
        }
    }

    return RT_EOK;
}

static rt_err_t w25n01gv_mtd_write_page(struct rt_mtd_nand_device *device,
                                        rt_off_t page,
                                        const rt_uint8_t *data,
                                        rt_uint32_t data_len,
                                        const rt_uint8_t *spare,
                                        rt_uint32_t spare_len)
{
    uint16_t block = (uint16_t)(page / W25N01GV_PAGES_PER_BLOCK);
    uint8_t page_in_block = (uint8_t)(page % W25N01GV_PAGES_PER_BLOCK);

    (void)device;

    if (block < W25N01GV_UFFS_BLOCK_FIRST ||
        block >= W25N01GV_UFFS_BLOCK_FIRST + W25N01GV_UFFS_BLOCK_COUNT)
    {
        return -RT_ERROR;
    }

    return w25n01gv_write_page_with_oob(block, page_in_block, data, data_len, spare, spare_len);
}

static rt_err_t w25n01gv_mtd_move_page(struct rt_mtd_nand_device *device, rt_off_t src_page, rt_off_t dst_page)
{
    (void)device;
    (void)src_page;
    (void)dst_page;

    return -RT_ENOSYS;
}

static rt_err_t w25n01gv_mtd_erase_block(struct rt_mtd_nand_device *device, rt_uint32_t block)
{
    (void)device;

    if (block < W25N01GV_UFFS_BLOCK_FIRST ||
        block >= W25N01GV_UFFS_BLOCK_FIRST + W25N01GV_UFFS_BLOCK_COUNT)
    {
        return -RT_ERROR;
    }

    return w25n01gv_erase_block((uint16_t)block);
}

static rt_err_t w25n01gv_mtd_check_block(struct rt_mtd_nand_device *device, rt_uint32_t block)
{
    uint8_t marker = 0x00;

    (void)device;

    if (block < W25N01GV_UFFS_BLOCK_FIRST ||
        block >= W25N01GV_UFFS_BLOCK_FIRST + W25N01GV_UFFS_BLOCK_COUNT)
    {
        return -RT_ERROR;
    }

    if (w25n01gv_read_bad_block_marker((uint16_t)block, &marker) != RT_EOK)
    {
        return -RT_ERROR;
    }

    return marker == 0xFFU ? RT_EOK : -RT_ERROR;
}

static rt_err_t w25n01gv_mtd_mark_badblock(struct rt_mtd_nand_device *device, rt_uint32_t block)
{
    uint8_t marker = 0x00;

    (void)device;

    if (block < W25N01GV_UFFS_BLOCK_FIRST ||
        block >= W25N01GV_UFFS_BLOCK_FIRST + W25N01GV_UFFS_BLOCK_COUNT)
    {
        return -RT_ERROR;
    }

    return w25n01gv_write_page_with_oob((uint16_t)block, 0, RT_NULL, 0, &marker, 1);
}

static const struct rt_mtd_nand_driver_ops w25n01gv_mtd_ops =
{
    w25n01gv_mtd_read_id,
    w25n01gv_mtd_read_page,
    w25n01gv_mtd_write_page,
    w25n01gv_mtd_move_page,
    w25n01gv_mtd_erase_block,
    w25n01gv_mtd_check_block,
    w25n01gv_mtd_mark_badblock,
};

static rt_err_t w25n01gv_mtd_register(void)
{
    memset(&w25n01gv_mtd_dev, 0, sizeof(w25n01gv_mtd_dev));
    w25n01gv_mtd_dev.page_size = W25N01GV_PAGE_SIZE;
    w25n01gv_mtd_dev.oob_size = W25N01GV_OOB_SIZE;
    w25n01gv_mtd_dev.oob_free = W25N01GV_OOB_SIZE;
    w25n01gv_mtd_dev.plane_num = 1;
    w25n01gv_mtd_dev.pages_per_block = W25N01GV_PAGES_PER_BLOCK;
    w25n01gv_mtd_dev.block_total = W25N01GV_BLOCK_COUNT;
    w25n01gv_mtd_dev.block_start = W25N01GV_UFFS_BLOCK_FIRST;
    w25n01gv_mtd_dev.block_end = W25N01GV_UFFS_BLOCK_FIRST + W25N01GV_UFFS_BLOCK_COUNT - 1U;
    w25n01gv_mtd_dev.ops = &w25n01gv_mtd_ops;

    return rt_mtd_nand_register_device(W25N01GV_UFFS_MTD_NAME, &w25n01gv_mtd_dev);
}

#if 0
/* 历史自制 FTL/块设备实现已停用；UFFS 独占整颗 NAND。 */
static void w25n01gv_ftl_make_meta(uint8_t *meta, uint32_t sector, uint32_t seq)
{
    memset(meta, 0xFF, W25N01GV_FTL_META_SIZE);
    meta[0] = W25N01GV_FTL_MAGIC0;
    meta[1] = W25N01GV_FTL_MAGIC1;
    meta[2] = (uint8_t)sector;
    meta[3] = (uint8_t)(sector >> 8);
    meta[4] = (uint8_t)(sector >> 16);
    meta[5] = (uint8_t)(sector >> 24);
    meta[6] = (uint8_t)seq;
    meta[7] = (uint8_t)(seq >> 8);
    meta[8] = (uint8_t)(seq >> 16);
    meta[9] = (uint8_t)(seq >> 24);
}

static int w25n01gv_ftl_parse_meta(const uint8_t *meta, uint32_t *sector, uint32_t *seq)
{
    if (meta == RT_NULL || sector == RT_NULL || seq == RT_NULL)
    {
        return 0;
    }

    if (meta[0] != W25N01GV_FTL_MAGIC0 || meta[1] != W25N01GV_FTL_MAGIC1)
    {
        return 0;
    }

    *sector = ((uint32_t)meta[2]) |
              ((uint32_t)meta[3] << 8) |
              ((uint32_t)meta[4] << 16) |
              ((uint32_t)meta[5] << 24);
    *seq = ((uint32_t)meta[6]) |
           ((uint32_t)meta[7] << 8) |
           ((uint32_t)meta[8] << 16) |
           ((uint32_t)meta[9] << 24);

    return w25n01gv_sector_is_valid(*sector);
}

static rt_err_t w25n01gv_ftl_scan(void)
{
    uint32_t sector;
    uint32_t seq;

    for (uint32_t i = 0; i < W25N01GV_FTL_LOGICAL_SECTORS; i++)
    {
        w25n01gv_sector_map[i] = W25N01GV_FTL_INVALID_PAGE;
    }

    w25n01gv_next_page = 0;
    w25n01gv_next_seq = 1;

    for (uint16_t page = 0; page < W25N01GV_FTL_TOTAL_PAGES; page++)
    {
        uint16_t block = w25n01gv_ftl_block_from_page(page);
        uint8_t page_in_block = w25n01gv_ftl_page_in_block(page);

        if (w25n01gv_read_oob(block,
                             page_in_block,
                             W25N01GV_FTL_META_COL,
                             w25n01gv_oob_buf,
                             W25N01GV_FTL_META_SIZE) != RT_EOK)
        {
            return -RT_ERROR;
        }

        if (w25n01gv_oob_buf[0] == 0xFFU && w25n01gv_oob_buf[1] == 0xFFU)
        {
            if (w25n01gv_next_page == page)
            {
                break;
            }
            continue;
        }

        w25n01gv_next_page = (uint16_t)(page + 1U);
        if (w25n01gv_ftl_parse_meta(w25n01gv_oob_buf, &sector, &seq))
        {
            w25n01gv_sector_map[sector] = page;
            if (seq >= w25n01gv_next_seq)
            {
                w25n01gv_next_seq = seq + 1U;
            }
        }
    }

    return RT_EOK;
}

static rt_err_t w25n01gv_ftl_format(void)
{
    for (uint16_t block = W25N01GV_FTL_BLOCK_FIRST;
         block < W25N01GV_FTL_BLOCK_FIRST + W25N01GV_FTL_BLOCK_COUNT;
         block++)
    {
        uint8_t marker = 0x00;

        if (w25n01gv_read_bad_block_marker(block, &marker) != RT_EOK)
        {
            return -RT_ERROR;
        }

        if (marker != 0xFFU)
        {
            if (w25n01gv_read_oob(block,
                                 0,
                                 W25N01GV_FTL_LEGACY_META_COL,
                                 w25n01gv_oob_buf,
                                 2) != RT_EOK)
            {
                return -RT_ERROR;
            }

            if (w25n01gv_oob_buf[0] != W25N01GV_FTL_MAGIC0 ||
                w25n01gv_oob_buf[1] != W25N01GV_FTL_MAGIC1)
            {
                LOG_W("skip bad block %u, marker=0x%02x", block, marker);
                continue;
            }

            LOG_I("reclaim legacy FTL marker on block %u", block);
        }

        if (w25n01gv_erase_block(block) != RT_EOK)
        {
            rt_kprintf("W25N01GV: erase FTL block %u failed\n", block);
            return -RT_ERROR;
        }
    }

    return w25n01gv_ftl_scan();
}

static rt_err_t w25n01gv_ftl_read_sector(uint32_t sector, uint8_t *buf)
{
    uint16_t physical_page;

    if (!w25n01gv_sector_is_valid(sector) || buf == RT_NULL)
    {
        return -RT_ERROR;
    }

    physical_page = w25n01gv_sector_map[sector];
    if (physical_page == W25N01GV_FTL_INVALID_PAGE)
    {
        memset(buf, 0xFF, W25N01GV_BLOCK_DEV_SECTOR_SIZE);
        return RT_EOK;
    }

    return w25n01gv_read_page(w25n01gv_ftl_block_from_page(physical_page),
                             w25n01gv_ftl_page_in_block(physical_page),
                             buf,
                             W25N01GV_BLOCK_DEV_SECTOR_SIZE);
}

static rt_err_t w25n01gv_ftl_write_sector(uint32_t sector, const uint8_t *buf)
{
    uint16_t block;
    uint8_t page;
    rt_err_t ret;
    uint8_t status = 0;

    if (!w25n01gv_sector_is_valid(sector) || buf == RT_NULL)
    {
        return -RT_ERROR;
    }

    while (w25n01gv_next_page < W25N01GV_FTL_TOTAL_PAGES)
    {
        block = w25n01gv_ftl_block_from_page(w25n01gv_next_page);
        page = w25n01gv_ftl_page_in_block(w25n01gv_next_page);

        if (page == 0U)
        {
            uint8_t marker = 0x00;
            if (w25n01gv_read_bad_block_marker(block, &marker) != RT_EOK || marker != 0xFFU)
            {
                w25n01gv_next_page = (uint16_t)(w25n01gv_next_page + W25N01GV_PAGES_PER_BLOCK);
                continue;
            }
        }

        memcpy(w25n01gv_page_buf, buf, W25N01GV_BLOCK_DEV_SECTOR_SIZE);
        memset(w25n01gv_page_buf + W25N01GV_BLOCK_DEV_SECTOR_SIZE,
               0xFF,
               W25N01GV_PAGE_SIZE - W25N01GV_BLOCK_DEV_SECTOR_SIZE);
        w25n01gv_ftl_make_meta(w25n01gv_oob_buf, sector, w25n01gv_next_seq);

        ret = w25n01gv_write_enable();
        if (ret != RT_EOK)
        {
            rt_kprintf("W25N01GV: FTL write enable failed\n");
            return ret;
        }

        if (w25n01gv_read_feature(W25N01GV_REG_STATUS, &status) != RT_EOK ||
            (status & W25N01GV_STATUS_WEL) == 0U)
        {
            rt_kprintf("W25N01GV: WEL not set before program, status=0x%02x\n", status);
            return -RT_ERROR;
        }

        ret = w25n01gv_program_load(0, w25n01gv_page_buf, W25N01GV_PAGE_SIZE);
        if (ret != RT_EOK)
        {
            rt_kprintf("W25N01GV: FTL program load data failed\n");
            return ret;
        }

        ret = w25n01gv_random_program_load(W25N01GV_PAGE_SIZE + W25N01GV_FTL_META_COL,
                                          w25n01gv_oob_buf,
                                          W25N01GV_FTL_META_SIZE);
        if (ret != RT_EOK)
        {
            rt_kprintf("W25N01GV: FTL program load meta failed\n");
            return ret;
        }

        ret = w25n01gv_program_execute(w25n01gv_row_from_block_page(block, page));
        if (ret != RT_EOK)
        {
            rt_kprintf("W25N01GV: FTL program execute failed, block=%u page=%u\n", block, page);
            return ret;
        }

        w25n01gv_sector_map[sector] = w25n01gv_next_page;
        w25n01gv_next_page++;
        w25n01gv_next_seq++;
        return RT_EOK;
    }

    rt_kprintf("W25N01GV: FTL area full, GC not implemented\n");
    return -RT_EFULL;
}

static rt_ssize_t w25n01gv_blk_read(rt_device_t dev, rt_off_t pos, void *buffer, rt_size_t size)
{
    uint8_t *buf = (uint8_t *)buffer;

    (void)dev;

    if (buffer == RT_NULL || pos < 0 || (uint32_t)pos + size > W25N01GV_FTL_LOGICAL_SECTORS)
    {
        return 0;
    }

    for (rt_size_t i = 0; i < size; i++)
    {
        if (w25n01gv_ftl_read_sector((uint32_t)pos + i,
                                    buf + (i * W25N01GV_BLOCK_DEV_SECTOR_SIZE)) != RT_EOK)
        {
            return 0;
        }
    }

    return size;
}

static rt_ssize_t w25n01gv_blk_write(rt_device_t dev, rt_off_t pos, const void *buffer, rt_size_t size)
{
    const uint8_t *buf = (const uint8_t *)buffer;

    (void)dev;

    if (buffer == RT_NULL || pos < 0 || (uint32_t)pos + size > W25N01GV_FTL_LOGICAL_SECTORS)
    {
        return 0;
    }

    for (rt_size_t i = 0; i < size; i++)
    {
        if (w25n01gv_ftl_write_sector((uint32_t)pos + i,
                                     buf + (i * W25N01GV_BLOCK_DEV_SECTOR_SIZE)) != RT_EOK)
        {
            return 0;
        }
    }

    return size;
}

static rt_err_t w25n01gv_blk_control(rt_device_t dev, int cmd, void *args)
{
    (void)dev;

    if (cmd == RT_DEVICE_CTRL_BLK_GETGEOME)
    {
        if (args == RT_NULL)
        {
            return -RT_ERROR;
        }
        memcpy(args, &w25n01gv_geometry, sizeof(w25n01gv_geometry));
        return RT_EOK;
    }

    if (cmd == RT_DEVICE_CTRL_BLK_ERASE)
    {
        return RT_EOK;
    }

    return -RT_ERROR;
}

static rt_err_t w25n01gv_blk_register(void)
{
    memset(&w25n01gv_blk_dev, 0, sizeof(w25n01gv_blk_dev));
    w25n01gv_blk_dev.type = RT_Device_Class_Block;
#ifdef RT_USING_DEVICE_OPS
    /* 当前 BSP 未启用 RT_USING_DEVICE_OPS，保留分支便于后续配置切换。 */
#else
    w25n01gv_blk_dev.init = RT_NULL;
    w25n01gv_blk_dev.open = RT_NULL;
    w25n01gv_blk_dev.close = RT_NULL;
    w25n01gv_blk_dev.read = w25n01gv_blk_read;
    w25n01gv_blk_dev.write = w25n01gv_blk_write;
    w25n01gv_blk_dev.control = w25n01gv_blk_control;
#endif
    w25n01gv_blk_dev.user_data = RT_NULL;

    return rt_device_register(&w25n01gv_blk_dev, W25N01GV_DEVICE_NAME, RT_DEVICE_FLAG_RDWR);
}
#endif

static void w25n01gv_print_id(void)
{
    uint8_t id[W25N01GV_JEDEC_ID_LEN] = {0};

    if (w25n01gv_read_jedec_id(id) != RT_EOK)
    {
        rt_kprintf("W25N01GV: read id failed\n");
        return;
    }

    rt_kprintf("W25N01GV JEDEC ID: %02x %02x %02x%s\n",
               id[0],
               id[1],
               id[2],
               w25n01gv_match_jedec_id(id) ? " (matched)" : " (not matched)");
}

static void w25n01gv_dump_hex(const uint8_t *buf, rt_size_t len)
{
    for (rt_size_t i = 0; i < len; i++)
    {
        if ((i % 16U) == 0U)
        {
            rt_kprintf("%04x: ", (unsigned int)i);
        }

        rt_kprintf("%02x ", buf[i]);

        if ((i % 16U) == 15U || i + 1U == len)
        {
            rt_kprintf("\n");
        }
    }
}

static int w25n01gv_parse_block_page(int argc, char **argv, uint16_t *block, uint8_t *page)
{
    char *end = RT_NULL;
    unsigned long value;

    if (argc < 4 || block == RT_NULL || page == RT_NULL)
    {
        return -RT_ERROR;
    }

    value = strtoul(argv[2], &end, 0);
    if (end == argv[2] || value >= W25N01GV_BLOCK_COUNT)
    {
        return -RT_ERROR;
    }
    *block = (uint16_t)value;

    value = strtoul(argv[3], &end, 0);
    if (end == argv[3] || value >= W25N01GV_PAGES_PER_BLOCK)
    {
        return -RT_ERROR;
    }
    *page = (uint8_t)value;

    return RT_EOK;
}

static int w25n01gv_msh(int argc, char **argv)
{
    uint8_t value = 0;

    if (argc < 2)
    {
        rt_kprintf("usage:\n");
        rt_kprintf("  w25n01gv id\n");
        rt_kprintf("  w25n01gv status\n");
        rt_kprintf("  w25n01gv regs\n");
        rt_kprintf("  w25n01gv stats [reset]\n");
        rt_kprintf("  w25n01gv unlock\n");
        rt_kprintf("  w25n01gv bb <block>\n");
        rt_kprintf("  w25n01gv read <block> <page> [len]\n");
        return 0;
    }

    if (strcmp(argv[1], "regs") == 0)
    {
        uint8_t lock = 0;
        uint8_t config = 0;
        uint8_t status = 0;

        if (w25n01gv_read_feature(W25N01GV_REG_BLOCK_LOCK, &lock) == RT_EOK &&
            w25n01gv_read_feature(W25N01GV_REG_CONFIG, &config) == RT_EOK &&
            w25n01gv_read_feature(W25N01GV_REG_STATUS, &status) == RT_EOK)
        {
            rt_kprintf("W25N01GV regs: lock=0x%02x config=0x%02x status=0x%02x\n",
                       lock,
                       config,
                       status);
        }
        else
        {
            rt_kprintf("W25N01GV: read regs failed\n");
        }
        return 0;
    }

    if (strcmp(argv[1], "stats") == 0)
    {
        if (argc > 2 && strcmp(argv[2], "reset") == 0)
        {
            w25n01gv_stats_reset();
        }
        rt_kprintf("W25N01GV stats: load=%u program=%u erase=%u busy=%u ms\n",
                   w25n01gv_stats.load_pages,
                   w25n01gv_stats.program_pages,
                   w25n01gv_stats.erase_blocks,
                   w25n01gv_stats.busy_wait_us / 1000U);
        rt_kprintf("W25N01GV bytes: cache_read=%u program_load=%u\n",
                   w25n01gv_stats.cache_read_bytes,
                   w25n01gv_stats.program_load_bytes);
        return 0;
    }

    if (strcmp(argv[1], "unlock") == 0)
    {
        if (w25n01gv_unlock_all_blocks() == RT_EOK)
        {
            rt_kprintf("W25N01GV: all blocks unlocked\n");
        }
        else
        {
            rt_kprintf("W25N01GV: unlock failed\n");
        }
        return 0;
    }

    if (strcmp(argv[1], "id") == 0)
    {
        w25n01gv_print_id();
        return 0;
    }

    if (strcmp(argv[1], "status") == 0)
    {
        if (w25n01gv_read_feature(W25N01GV_REG_STATUS, &value) == RT_EOK)
        {
            rt_kprintf("W25N01GV status: 0x%02x, ecc=%d, busy=%d\n",
                       value,
                       w25n01gv_decode_ecc_status(value),
                       (value & W25N01GV_STATUS_BUSY) ? 1 : 0);
        }
        else
        {
            rt_kprintf("W25N01GV: read status failed\n");
        }
        return 0;
    }

    if (strcmp(argv[1], "bb") == 0)
    {
        uint16_t block;
        char *end = RT_NULL;

        if (argc < 3)
        {
            rt_kprintf("usage: w25n01gv bb <block>\n");
            return -RT_ERROR;
        }

        block = (uint16_t)strtoul(argv[2], &end, 0);
        if (end == argv[2] || block >= W25N01GV_BLOCK_COUNT)
        {
            rt_kprintf("W25N01GV: invalid block\n");
            return -RT_ERROR;
        }

        if (w25n01gv_read_bad_block_marker(block, &value) == RT_EOK)
        {
            rt_kprintf("W25N01GV block %u marker: 0x%02x (%s)\n",
                       block,
                       value,
                       value == 0xFFU ? "good" : "bad");
        }
        else
        {
            rt_kprintf("W25N01GV: read bad block marker failed\n");
        }
        return 0;
    }

#if 0
    /* UFFS 独占 NAND 后禁止绕过文件系统直接擦除。 */
    if (strcmp(argv[1], "erase") == 0)
    {
        uint16_t block;
        char *end = RT_NULL;

        if (argc < 3)
        {
            rt_kprintf("usage: w25n01gv erase <block>\n");
            return -RT_ERROR;
        }

        block = (uint16_t)strtoul(argv[2], &end, 0);
        if (end == argv[2] || block >= W25N01GV_BLOCK_COUNT)
        {
            rt_kprintf("W25N01GV: invalid block\n");
            return -RT_ERROR;
        }

        if (w25n01gv_erase_block(block) == RT_EOK)
        {
            rt_kprintf("W25N01GV: erase block %u ok\n", block);
        }
        else
        {
            rt_kprintf("W25N01GV: erase block %u failed\n", block);
            return -RT_ERROR;
        }
        return 0;
    }
#endif

#if 0
    if (strcmp(argv[1], "blk_info") == 0)
    {
        rt_kprintf("W25N01GV block dev: name=%s sector=%u count=%u next_page=%u seq=%u\n",
                   W25N01GV_DEVICE_NAME,
                   W25N01GV_BLOCK_DEV_SECTOR_SIZE,
                   W25N01GV_FTL_LOGICAL_SECTORS,
                   w25n01gv_next_page,
                   w25n01gv_next_seq);
        return 0;
    }

    if (strcmp(argv[1], "ftl_format") == 0)
    {
        rt_kprintf("W25N01GV: erase FTL blocks %u..%u\n",
                   W25N01GV_FTL_BLOCK_FIRST,
                   W25N01GV_FTL_BLOCK_FIRST + W25N01GV_FTL_BLOCK_COUNT - 1U);
        if (w25n01gv_ftl_format() == RT_EOK)
        {
            rt_kprintf("W25N01GV: FTL format ok, next_page=%u seq=%u\n",
                       w25n01gv_next_page,
                       w25n01gv_next_seq);
        }
        else
        {
            rt_kprintf("W25N01GV: FTL format failed\n");
        }
        return 0;
    }

    if (strcmp(argv[1], "uffs_format") == 0)
    {
        rt_kprintf("W25N01GV: erase UFFS blocks %u..%u\n",
                   W25N01GV_UFFS_BLOCK_FIRST,
                   W25N01GV_UFFS_BLOCK_FIRST + W25N01GV_UFFS_BLOCK_COUNT - 1U);
        for (uint16_t block = W25N01GV_UFFS_BLOCK_FIRST;
             block < W25N01GV_UFFS_BLOCK_FIRST + W25N01GV_UFFS_BLOCK_COUNT;
             block++)
        {
            if (w25n01gv_mtd_check_block(&w25n01gv_mtd_dev, block) != RT_EOK)
            {
                rt_kprintf("W25N01GV: skip bad UFFS block %u\n", block);
                continue;
            }

            if (w25n01gv_erase_block(block) != RT_EOK)
            {
                rt_kprintf("W25N01GV: erase UFFS block %u failed\n", block);
                return -RT_ERROR;
            }
        }
        rt_kprintf("W25N01GV: UFFS format ok\n");
        return 0;
    }

    if (strcmp(argv[1], "blk_test") == 0)
    {
        rt_device_t dev;
        uint32_t sector;
        char *end = RT_NULL;

        if (argc < 3)
        {
            rt_kprintf("usage: w25n01gv blk_test <sector>\n");
            return -RT_ERROR;
        }

        sector = strtoul(argv[2], &end, 0);
        if (end == argv[2] || !w25n01gv_sector_is_valid(sector))
        {
            rt_kprintf("W25N01GV: invalid sector\n");
            return -RT_ERROR;
        }

        dev = rt_device_find(W25N01GV_DEVICE_NAME);
        if (dev == RT_NULL)
        {
            rt_kprintf("W25N01GV: block device not found\n");
            return -RT_ERROR;
        }

        if (rt_device_open(dev, RT_DEVICE_OFLAG_RDWR) != RT_EOK)
        {
            rt_kprintf("W25N01GV: block device open failed\n");
            return -RT_ERROR;
        }

        w25n01gv_fill_test_pattern(w25n01gv_page_buf, W25N01GV_BLOCK_DEV_SECTOR_SIZE, (uint16_t)sector, 0);
        if (rt_device_write(dev, sector, w25n01gv_page_buf, 1) != 1)
        {
            rt_kprintf("W25N01GV: block write failed\n");
            rt_device_close(dev);
            return -RT_ERROR;
        }

        memset(w25n01gv_page_buf, 0, W25N01GV_BLOCK_DEV_SECTOR_SIZE);
        if (rt_device_read(dev, sector, w25n01gv_page_buf, 1) != 1)
        {
            rt_kprintf("W25N01GV: block read failed\n");
            rt_device_close(dev);
            return -RT_ERROR;
        }

        rt_device_close(dev);

        if (!w25n01gv_check_test_pattern(w25n01gv_page_buf, W25N01GV_BLOCK_DEV_SECTOR_SIZE, (uint16_t)sector, 0))
        {
            rt_kprintf("W25N01GV: block verify mismatch\n");
            w25n01gv_dump_hex(w25n01gv_page_buf, 64);
            return -RT_ERROR;
        }

        rt_kprintf("W25N01GV: block test ok, sector=%u\n", sector);
        return 0;
    }

    if (strcmp(argv[1], "blk_read") == 0)
    {
        rt_device_t dev;
        uint32_t sector;
        rt_size_t len = 64;
        char *end = RT_NULL;

        if (argc < 3)
        {
            rt_kprintf("usage: w25n01gv blk_read <sector> [len]\n");
            return -RT_ERROR;
        }

        sector = strtoul(argv[2], &end, 0);
        if (end == argv[2] || !w25n01gv_sector_is_valid(sector))
        {
            rt_kprintf("W25N01GV: invalid sector\n");
            return -RT_ERROR;
        }

        if (argc >= 4)
        {
            len = (rt_size_t)strtoul(argv[3], &end, 0);
            if (end == argv[3] || len == 0U || len > W25N01GV_READ_DUMP_MAX)
            {
                rt_kprintf("W25N01GV: len must be 1..%u\n", W25N01GV_READ_DUMP_MAX);
                return -RT_ERROR;
            }
        }

        dev = rt_device_find(W25N01GV_DEVICE_NAME);
        if (dev == RT_NULL)
        {
            rt_kprintf("W25N01GV: block device not found\n");
            return -RT_ERROR;
        }

        if (rt_device_open(dev, RT_DEVICE_OFLAG_RDONLY) != RT_EOK)
        {
            rt_kprintf("W25N01GV: block device open failed\n");
            return -RT_ERROR;
        }

        memset(w25n01gv_page_buf, 0, W25N01GV_BLOCK_DEV_SECTOR_SIZE);
        if (rt_device_read(dev, sector, w25n01gv_page_buf, 1) != 1)
        {
            rt_kprintf("W25N01GV: block read failed\n");
            rt_device_close(dev);
            return -RT_ERROR;
        }

        rt_device_close(dev);

        rt_kprintf("W25N01GV block read sector=%u len=%u\n", sector, (unsigned int)len);
        w25n01gv_dump_hex(w25n01gv_page_buf, len);
        return 0;
    }
#endif

    if (strcmp(argv[1], "read") == 0)
    {
        uint16_t block;
        uint8_t page;
        rt_size_t len = 64;
        char *end = RT_NULL;

        if (w25n01gv_parse_block_page(argc, argv, &block, &page) != RT_EOK)
        {
            rt_kprintf("usage: w25n01gv read <block> <page> [len]\n");
            return -RT_ERROR;
        }

        if (argc >= 5)
        {
            len = (rt_size_t)strtoul(argv[4], &end, 0);
            if (end == argv[4] || len == 0U || len > W25N01GV_READ_DUMP_MAX)
            {
                rt_kprintf("W25N01GV: len must be 1..%u\n", W25N01GV_READ_DUMP_MAX);
                return -RT_ERROR;
            }
        }

        if (w25n01gv_read_page(block, page, w25n01gv_page_buf, len) == RT_EOK)
        {
            rt_kprintf("W25N01GV read block=%u page=%u len=%u\n", block, page, (unsigned int)len);
            w25n01gv_dump_hex(w25n01gv_page_buf, len);
        }
        else
        {
            rt_kprintf("W25N01GV: read page failed\n");
        }
        return 0;
    }

#if 0
    /* UFFS 独占 NAND 后禁止绕过文件系统直接写页。 */
    if (strcmp(argv[1], "write_test") == 0)
    {
        uint16_t block;
        uint8_t page = 0;
        char *end = RT_NULL;

        if (argc < 3)
        {
            rt_kprintf("usage: w25n01gv write_test <block> [page]\n");
            return -RT_ERROR;
        }

        block = (uint16_t)strtoul(argv[2], &end, 0);
        if (end == argv[2] || block >= W25N01GV_BLOCK_COUNT)
        {
            rt_kprintf("W25N01GV: invalid block\n");
            return -RT_ERROR;
        }

        if (argc >= 4)
        {
            unsigned long page_value = strtoul(argv[3], &end, 0);
            if (end == argv[3] || page_value >= W25N01GV_PAGES_PER_BLOCK)
            {
                rt_kprintf("W25N01GV: invalid page\n");
                return -RT_ERROR;
            }
            page = (uint8_t)page_value;
        }

        if (block < W25N01GV_TEST_BLOCK_FIRST)
        {
            rt_kprintf("W25N01GV: write_test only allows block %u..%u\n",
                       W25N01GV_TEST_BLOCK_FIRST,
                       W25N01GV_BLOCK_COUNT - 1U);
            return -RT_ERROR;
        }

        if (w25n01gv_read_bad_block_marker(block, &value) != RT_EOK || value != 0xFFU)
        {
            rt_kprintf("W25N01GV: block %u is not usable, marker=0x%02x\n", block, value);
            return -RT_ERROR;
        }

        rt_kprintf("W25N01GV: erase block %u\n", block);
        if (w25n01gv_erase_block(block) != RT_EOK)
        {
            rt_kprintf("W25N01GV: erase failed\n");
            return -RT_ERROR;
        }

        w25n01gv_fill_test_pattern(w25n01gv_page_buf, W25N01GV_PAGE_SIZE, block, page);
        rt_kprintf("W25N01GV: program block %u page %u\n", block, page);
        if (w25n01gv_write_page(block, page, w25n01gv_page_buf, W25N01GV_PAGE_SIZE) != RT_EOK)
        {
            rt_kprintf("W25N01GV: program failed\n");
            return -RT_ERROR;
        }

        memset(w25n01gv_page_buf, 0, sizeof(w25n01gv_page_buf));
        if (w25n01gv_read_page(block, page, w25n01gv_page_buf, W25N01GV_PAGE_SIZE) != RT_EOK)
        {
            rt_kprintf("W25N01GV: verify read failed\n");
            return -RT_ERROR;
        }

        if (!w25n01gv_check_test_pattern(w25n01gv_page_buf, W25N01GV_PAGE_SIZE, block, page))
        {
            rt_kprintf("W25N01GV: verify mismatch\n");
            w25n01gv_dump_hex(w25n01gv_page_buf, 64);
            return -RT_ERROR;
        }

        rt_kprintf("W25N01GV: write_test ok, block=%u page=%u\n", block, page);
        return 0;
    }
#endif

    rt_kprintf("W25N01GV: unknown command\n");
    return -RT_ERROR;
}
MSH_CMD_EXPORT_ALIAS(w25n01gv_msh, w25n01gv, W25N01GV communication diagnostics);

static int rt_hw_w25n01gv_init(void)
{
    struct rt_spi_configuration cfg;
    uint8_t id[W25N01GV_JEDEC_ID_LEN] = {0};

    if (rt_hw_spi_device_attach(W25N01GV_SPI_BUS_NAME, W25N01GV_SPI_DEVICE_NAME, GET_PIN(B, 1)) != RT_EOK)
    {
        rt_kprintf("W25N01GV: attach %s on %s failed\n", W25N01GV_SPI_DEVICE_NAME, W25N01GV_SPI_BUS_NAME);
        return -RT_ERROR;
    }

    w25n01gv_spi_dev = (struct rt_spi_device *)rt_device_find(W25N01GV_SPI_DEVICE_NAME);
    if (w25n01gv_spi_dev == RT_NULL)
    {
        rt_kprintf("W25N01GV: spi device not found\n");
        return -RT_ERROR;
    }

    cfg.data_width = 8;
    cfg.mode = RT_SPI_MODE_0 | RT_SPI_MSB;
    cfg.max_hz = W25N01GV_SPI_MAX_HZ;
    if (rt_spi_configure(w25n01gv_spi_dev, &cfg) != RT_EOK)
    {
        rt_kprintf("W25N01GV: spi configure failed\n");
        return -RT_ERROR;
    }

    if (w25n01gv_reset() != RT_EOK)
    {
        rt_kprintf("W25N01GV: reset failed\n");
        return -RT_ERROR;
    }

    if (w25n01gv_read_jedec_id(id) != RT_EOK || !w25n01gv_match_jedec_id(id))
    {
        rt_kprintf("W25N01GV: id mismatch: %02x %02x %02x\n", id[0], id[1], id[2]);
        return -RT_ERROR;
    }

    LOG_I("init ok, id=%02x %02x %02x, capacity=%u KiB",
          id[0], id[1], id[2], W25N01GV_CAPACITY / 1024U);

    if (w25n01gv_unlock_all_blocks() != RT_EOK)
    {
        rt_kprintf("W25N01GV: unlock for block device failed\n");
        return -RT_ERROR;
    }

    if (w25n01gv_configure_for_raw_oob() != RT_EOK)
    {
        rt_kprintf("W25N01GV: raw OOB config failed\n");
        return -RT_ERROR;
    }

    if (w25n01gv_mtd_register() != RT_EOK)
    {
        rt_kprintf("W25N01GV: MTD NAND device register failed\n");
        return -RT_ERROR;
    }

    LOG_I("MTD NAND device '%s' ready, blocks %u..%u",
          W25N01GV_UFFS_MTD_NAME,
          W25N01GV_UFFS_BLOCK_FIRST,
          W25N01GV_UFFS_BLOCK_FIRST + W25N01GV_UFFS_BLOCK_COUNT - 1U);

    return RT_EOK;
}
INIT_COMPONENT_EXPORT(rt_hw_w25n01gv_init);

#endif /* BSP_USING_SPI_FLASH */

int w25n01gv_match_jedec_id(const uint8_t id[W25N01GV_JEDEC_ID_LEN])
{
    if (id == 0)
    {
        return 0;
    }

    return id[0] == W25N01GV_MF_ID_WINBOND &&
           id[1] == W25N01GV_DEVICE_ID_0 &&
           id[2] == W25N01GV_DEVICE_ID_1;
}

uint32_t w25n01gv_row_from_block_page(uint16_t block, uint8_t page)
{
    return ((uint32_t)block * W25N01GV_PAGES_PER_BLOCK) + page;
}

enum w25n01gv_ecc_status w25n01gv_decode_ecc_status(uint8_t status)
{
    switch (status & W25N01GV_STATUS_ECC_MASK)
    {
    case 0x00:
        return W25N01GV_ECC_OK;
    case 0x10:
        return W25N01GV_ECC_CORRECTED;
    case 0x20:
        return W25N01GV_ECC_FAILED;
    default:
        return W25N01GV_ECC_RESERVED;
    }
}

void w25n01gv_fill_test_pattern(uint8_t *buf, uint32_t len, uint16_t block, uint8_t page)
{
    if (buf == 0)
    {
        return;
    }

    for (uint32_t i = 0; i < len; i++)
    {
        buf[i] = (uint8_t)(0xA5U ^ (uint8_t)i ^ (uint8_t)(i >> 8) ^
                           (uint8_t)block ^ (uint8_t)(block >> 8) ^ page);
    }
}

int w25n01gv_check_test_pattern(const uint8_t *buf, uint32_t len, uint16_t block, uint8_t page)
{
    if (buf == 0)
    {
        return 0;
    }

    for (uint32_t i = 0; i < len; i++)
    {
        uint8_t expected = (uint8_t)(0xA5U ^ (uint8_t)i ^ (uint8_t)(i >> 8) ^
                                    (uint8_t)block ^ (uint8_t)(block >> 8) ^ page);
        if (buf[i] != expected)
        {
            return 0;
        }
    }

    return 1;
}
