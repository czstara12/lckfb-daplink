/*
 * Copyright (c) 2006-2024, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2024-03-19     yuanzihao    first implementation
 */

#include <stdio.h>
#include <rtthread.h>
#include <rtdevice.h>
#include <board.h>
#include <dfs_fs.h>
#include <drv_sdio.h>
#include <dfs_posix.h>
#include <drv_gpio.h>
#ifndef RT_USING_NANO
#include <rtdevice.h>
#endif /* RT_USING_NANO */

#define LOG_TAG     "sd_card"     // 该模块对应的标签。不定义时，默认：NO_TAG
#define LOG_LVL     LOG_LVL_DBG   // 该模块对应的日志输出级别。不定义时，默认：调试级别
#include <ulog.h>                 // 必须在 LOG_TAG 与 LOG_LVL 下面

#define THREAD_PRIORITY 10
#define THREAD_STACK_SIZE 1024
#define THREAD_TIMESLICE 5

#define SD_CARD_DET_PIN GET_PIN(D, 3)

static rt_thread_t sd_card_tid = RT_NULL;

extern int rt_hw_sdio_init(void);

int onboard_sdcard_mount(void)
{
    static rt_device_t device;

    device = rt_device_find("sd0");

	if(device == RT_NULL)
	{
		rt_hw_sdio_init();
		rt_thread_mdelay(200);
		device = rt_device_find("sd0");
	}
	
    if (device != RT_NULL)
    {
        if (dfs_mount("sd0", "/", "elm", 0, 0) == RT_EOK)
        {
            LOG_I("SD card mount to '/'");
            return RT_EOK;
        }
        LOG_E("SD card mount to '/' failed!");
        return RT_ERROR;
    }
	return 0;
}

int onboard_sdcard_unmount(void)
{
    static rt_device_t device;

    device = rt_device_find("sd0");

    if (dfs_unmount("/") == RT_EOK)
    {
        LOG_I("SD card unmount success");
        rt_thread_mdelay(200);
        return RT_EOK;
    }
    LOG_E("SD card unmount failed!");
    return RT_ERROR;
}

static void sd_card_thread_entry(void *param)
{
    static rt_int8_t _sd_card_state = PIN_HIGH;
    // 初始化SD卡检测引脚
    rt_pin_mode(SD_CARD_DET_PIN, PIN_MODE_INPUT_PULLUP);
	
	rt_thread_mdelay(500);
	
    while (1)
    {
        if ((rt_pin_read(SD_CARD_DET_PIN) == PIN_LOW)&&(_sd_card_state == PIN_HIGH))
        {
            LOG_I("SD card insert!");
            if (onboard_sdcard_mount() != RT_EOK)
            {
                LOG_E("onboard_sdcard_mount failed!");
            }
        }
        if ((rt_pin_read(SD_CARD_DET_PIN) == PIN_HIGH)&&(_sd_card_state == PIN_LOW))
        {
            LOG_I("SD card remove!");
			stm32_mmcsd_change();
            if (onboard_sdcard_unmount() != RT_EOK)
            {
                LOG_E("onboard_sdcard_unmount failed!");
            }
        }
        _sd_card_state = rt_pin_read(SD_CARD_DET_PIN);
        rt_thread_mdelay(200);
    }
}

int sd_card_thread_start(void)
{
    /* 创建线程，sd_card，入口是sd_card_thread_entry*/
    sd_card_tid = rt_thread_create("sd_card",
                                   sd_card_thread_entry, RT_NULL,
                            THREAD_STACK_SIZE,
                            THREAD_PRIORITY, THREAD_TIMESLICE);
    /* 如果获得线程控制块，启动这个线程 */
    if (sd_card_tid != RT_NULL)
        rt_thread_startup(sd_card_tid);

    return 0;
}
INIT_APP_EXPORT(sd_card_thread_start);

