/**
 * @file fal_spi_flash_sfud_port.c
 * @brief W25Q32 的 FAL 适配，设备不可用时返回错误。
 * SPDX-License-Identifier: Apache-2.0
 * Origin: restored from the BSP FAL SFUD port
 * Created-By: xcwynya
 */

#include <fal.h>
#include <sfud.h>
#include <dev_spi_flash_sfud.h>

static int init(void);
static int read(long offset, uint8_t *buf, size_t size);
static int write(long offset, const uint8_t *buf, size_t size);
static int erase(long offset, size_t size);

static sfud_flash_t sfud_dev;

/** @brief W25Q32 的 FAL 设备定义。 */
struct fal_flash_dev w25q32 =
{
    .name = "W25Q32",
    .addr = 0,
    .len = 4 * 1024 * 1024,
    .blk_size = 4096,
    .ops = {init, read, write, erase},
    .write_gran = 1
};

/** @brief 查找并初始化 FAL 后端，设备不存在或未就绪时返回 -1。 */
static int init(void)
{
    sfud_dev = rt_sfud_flash_find_by_dev_name("W25Q32");
    if (sfud_dev == RT_NULL || !sfud_dev->init_ok)
    {
        sfud_dev = RT_NULL;
        return -1;
    }

    w25q32.blk_size = sfud_dev->chip.erase_gran;
    w25q32.len = sfud_dev->chip.capacity;
    return 0;
}

/** @brief 读取 Flash，成功返回字节数，设备不可用或读取失败返回 -1。 */
static int read(long offset, uint8_t *buf, size_t size)
{
    if (sfud_dev == RT_NULL || !sfud_dev->init_ok)
    {
        return -1;
    }

    return sfud_read(sfud_dev, w25q32.addr + offset, size, buf) == SFUD_SUCCESS ? (int)size : -1;
}

/** @brief 写入 Flash，成功返回字节数，设备不可用或写入失败返回 -1。 */
static int write(long offset, const uint8_t *buf, size_t size)
{
    if (sfud_dev == RT_NULL || !sfud_dev->init_ok)
    {
        return -1;
    }

    return sfud_write(sfud_dev, w25q32.addr + offset, size, buf) == SFUD_SUCCESS ? (int)size : -1;
}

/** @brief 擦除 Flash，成功返回字节数，设备不可用或擦除失败返回 -1。 */
static int erase(long offset, size_t size)
{
    if (sfud_dev == RT_NULL || !sfud_dev->init_ok)
    {
        return -1;
    }

    return sfud_erase(sfud_dev, w25q32.addr + offset, size) == SFUD_SUCCESS ? (int)size : -1;
}
