/*
 * SPDX-License-Identifier: Apache-2.0
 * Origin: restored from the BSP SPI NOR implementation
 * Created-By: xcwynya
 */

#include <rtthread.h>
#include <dev_spi_flash.h>
#include <dev_spi_flash_sfud.h>
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

#endif /* BSP_USING_SPI_FLASH */
