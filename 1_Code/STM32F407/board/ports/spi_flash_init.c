/**
 * @file spi_flash_init.c
 * @brief W25Q32 初始化及无文件系统时的整片测速。
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

static void w25q32_retry_delay(void)
{
    rt_hw_us_delay(100U);
}

static int rt_hw_spi_flash_init(void)
{
    rt_spi_flash_device_t device;
    sfud_flash_t flash;

    if (rt_hw_spi_device_attach("spi1", "spi10", GET_PIN(B, 1)) != RT_EOK)
    {
        return -RT_ERROR;
    }

    device = rt_sfud_flash_probe("W25Q32", "spi10");
    if (device == RT_NULL)
    {
        return -RT_ERROR;
    }
    flash = rt_sfud_flash_find_by_dev_name("W25Q32");
    if (flash == RT_NULL || !flash->init_ok)
    {
        return -RT_ERROR;
    }
    flash->retry.delay = w25q32_retry_delay;

    return RT_EOK;
}
INIT_COMPONENT_EXPORT(rt_hw_spi_flash_init);

#ifndef BSP_USING_FLASH_LITTLEFS
#define W25Q32_TEST_BLOCK_SIZE 4096U

static void print_speed(const char *name, rt_uint32_t bytes, rt_uint32_t cycles)
{
    rt_uint32_t microseconds;
    rt_uint32_t kib_per_second_x100;

    if (cycles == 0U)
    {
        cycles = 1U;
    }
    microseconds = (rt_uint32_t)((rt_uint64_t)cycles * 1000000U / SystemCoreClock);
    kib_per_second_x100 = (rt_uint32_t)((rt_uint64_t)bytes * SystemCoreClock * 100U /
                                        cycles / 1024U);
    rt_kprintf("%s: %u us, %u.%02u KiB/s, %u cycles\n", name, microseconds,
               kib_per_second_x100 / 100U, kib_per_second_x100 % 100U, cycles);
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
    rt_uint32_t start;
    rt_uint32_t block_start;
    rt_uint32_t block_cycles;
    rt_uint32_t min_write_cycles = UINT32_MAX;
    rt_uint32_t max_write_cycles = 0U;
    rt_uint32_t erase_cycles;
    rt_uint32_t write_cycles;
    rt_uint32_t read_cycles;

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

    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
    rt_kprintf("testing %u bytes, all data will be erased\n", flash->chip.capacity);
    start = DWT->CYCCNT;
    if (sfud_erase(flash, 0U, flash->chip.capacity) != SFUD_SUCCESS)
    {
        rt_kprintf("erase failed\n");
        goto exit;
    }
    erase_cycles = DWT->CYCCNT - start;

    for (index = 0U; index < W25Q32_TEST_BLOCK_SIZE; index++)
    {
        buffer[index] = (rt_uint8_t)index;
    }
    start = DWT->CYCCNT;
    for (address = 0U; address < flash->chip.capacity; address += W25Q32_TEST_BLOCK_SIZE)
    {
        block_start = DWT->CYCCNT;
        if (sfud_write(flash, address, W25Q32_TEST_BLOCK_SIZE, buffer) != SFUD_SUCCESS)
        {
            rt_kprintf("write failed at 0x%08x\n", address);
            goto exit;
        }
        block_cycles = DWT->CYCCNT - block_start;
        if (block_cycles < min_write_cycles)
        {
            min_write_cycles = block_cycles;
        }
        if (block_cycles > max_write_cycles)
        {
            max_write_cycles = block_cycles;
        }
    }
    write_cycles = DWT->CYCCNT - start;

    start = DWT->CYCCNT;
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
    read_cycles = DWT->CYCCNT - start;

    print_speed("erase", flash->chip.capacity, erase_cycles);
    print_speed("write", flash->chip.capacity, write_cycles);
    print_speed("read ", flash->chip.capacity, read_cycles);
    rt_kprintf("write 4KiB: min %u us, max %u us\n",
               (rt_uint32_t)((rt_uint64_t)min_write_cycles * 1000000U / SystemCoreClock),
               (rt_uint32_t)((rt_uint64_t)max_write_cycles * 1000000U / SystemCoreClock));
    rt_kprintf("verify passed\n");

exit:
    rt_free(buffer);
}
MSH_CMD_EXPORT(w25q32_speed, erase/write/read full W25Q32 and show speed);
#endif /* !BSP_USING_FLASH_LITTLEFS */

#endif /* BSP_USING_SPI_FLASH */
