/*
 * SPDX-License-Identifier: Apache-2.0
 * Origin: restored from the BSP SPI NOR implementation
 * Created-By: xcwynya
 */

#include <rtthread.h>
#include <dev_spi_flash.h>
#include <dev_spi_flash_sfud.h>
#include <sfud.h>
#include <drv_spi.h>
#include <drv_gpio.h>

#ifdef BSP_USING_SPI_FLASH

static int rt_hw_spi_flash_init(void)
{
    if (rt_hw_spi_device_attach("spi1", "spi10", GET_PIN(B, 1)) != RT_EOK)
    {
        return -RT_ERROR;
    }

    return rt_sfud_flash_probe("W25Q32", "spi10") == RT_NULL ? -RT_ERROR : RT_EOK;
}
INIT_COMPONENT_EXPORT(rt_hw_spi_flash_init);

#define W25Q32_TEST_BLOCK_SIZE 4096U

static void print_speed(const char *name, rt_uint32_t bytes, rt_tick_t ticks)
{
    rt_uint32_t milliseconds;
    rt_uint32_t kib_per_second;

    if (ticks == 0U)
    {
        ticks = 1U;
    }
    milliseconds = (rt_uint32_t)((rt_uint64_t)ticks * 1000U / RT_TICK_PER_SECOND);
    kib_per_second = (rt_uint32_t)((rt_uint64_t)bytes * RT_TICK_PER_SECOND /
                                   ticks / 1024U);
    rt_kprintf("%s: %u ms, %u KiB/s\n", name, milliseconds, kib_per_second);
}

/**
 * @brief 擦除、写入、读取并校验整片 W25Q32，同时输出各阶段速度。
 *
 * @warning 该命令会覆盖 W25Q32 中的全部数据。
 */
static void w25q32_speed(void)
{
    sfud_flash_t flash = rt_sfud_flash_find_by_dev_name("W25Q32");
    struct rt_spi_device *spi_device;
    rt_uint8_t *buffer;
    rt_uint32_t address;
    rt_uint32_t index;
    rt_tick_t start;
    rt_tick_t erase_ticks;
    rt_tick_t write_ticks;
    rt_tick_t read_ticks;

    if (flash == RT_NULL)
    {
        rt_uint8_t command = 0x9FU;
        rt_uint8_t jedec_id[3] = {0U};

        spi_device = (struct rt_spi_device *)rt_device_find("spi10");
        if (spi_device != RT_NULL &&
            rt_spi_send_then_recv(spi_device, &command, 1U, jedec_id,
                                  sizeof(jedec_id)) == RT_EOK)
        {
            rt_kprintf("W25Q32 not found, JEDEC ID: %02x %02x %02x\n",
                       jedec_id[0], jedec_id[1], jedec_id[2]);
        }
        else
        {
            rt_kprintf("W25Q32 not found, JEDEC read failed\n");
        }
        return;
    }
    buffer = rt_malloc(W25Q32_TEST_BLOCK_SIZE);
    if (buffer == RT_NULL)
    {
        rt_kprintf("allocate test buffer failed\n");
        return;
    }

    rt_kprintf("testing %u bytes, all data will be erased\n", flash->chip.capacity);
    start = rt_tick_get();
    if (sfud_erase(flash, 0U, flash->chip.capacity) != SFUD_SUCCESS)
    {
        rt_kprintf("erase failed\n");
        goto exit;
    }
    erase_ticks = rt_tick_get() - start;

    for (index = 0U; index < W25Q32_TEST_BLOCK_SIZE; index++)
    {
        buffer[index] = (rt_uint8_t)index;
    }
    start = rt_tick_get();
    for (address = 0U; address < flash->chip.capacity; address += W25Q32_TEST_BLOCK_SIZE)
    {
        if (sfud_write(flash, address, W25Q32_TEST_BLOCK_SIZE, buffer) != SFUD_SUCCESS)
        {
            rt_kprintf("write failed at 0x%08x\n", address);
            goto exit;
        }
    }
    write_ticks = rt_tick_get() - start;

    start = rt_tick_get();
    for (address = 0U; address < flash->chip.capacity; address += W25Q32_TEST_BLOCK_SIZE)
    {
        if (sfud_read(flash, address, W25Q32_TEST_BLOCK_SIZE, buffer) != SFUD_SUCCESS)
        {
            rt_kprintf("read failed at 0x%08x\n", address);
            goto exit;
        }
        for (index = 0U; index < W25Q32_TEST_BLOCK_SIZE; index++)
        {
            if (buffer[index] != (rt_uint8_t)index)
            {
                rt_kprintf("verify failed at 0x%08x\n", address + index);
                goto exit;
            }
        }
    }
    read_ticks = rt_tick_get() - start;

    print_speed("erase", flash->chip.capacity, erase_ticks);
    print_speed("write", flash->chip.capacity, write_ticks);
    print_speed("read ", flash->chip.capacity, read_ticks);
    rt_kprintf("verify passed\n");

exit:
    rt_free(buffer);
}
MSH_CMD_EXPORT(w25q32_speed, erase/write/read full W25Q32 and show speed);

#endif /* BSP_USING_SPI_FLASH */
