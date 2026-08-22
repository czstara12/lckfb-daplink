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
#define SD_CARD_MOUNT_PATH "/sdcard"

static struct rt_thread sd_card_thread;
static rt_uint8_t sd_card_stack[THREAD_STACK_SIZE];

/**
 * @brief 挂载板载 SD 卡文件系统。
 *
 * @return RT_EOK 表示挂载成功，RT_ERROR 表示设备不存在或挂载失败。
 */
int onboard_sdcard_mount(void)
{
    rt_device_t device;
    rt_uint8_t retry;

    device = rt_device_find("sd0");

    if (device == RT_NULL)
    {
        for (retry = 0; retry < 10 && device == RT_NULL; retry++)
        {
            rt_thread_mdelay(50);
            device = rt_device_find("sd0");
        }
    }

    if (device == RT_NULL)
    {
        stm32_mmcsd_change();
        for (retry = 0; retry < 10 && device == RT_NULL; retry++)
        {
            rt_thread_mdelay(50);
            device = rt_device_find("sd0");
        }
    }

    if (device == RT_NULL)
    {
        return RT_ERROR;
    }

    if (dfs_filesystem_get_mounted_path(device) != RT_NULL ||
        dfs_mount("sd0", SD_CARD_MOUNT_PATH, "elm", 0, 0) == RT_EOK)
    {
        LOG_I("SD card mount to '%s'", SD_CARD_MOUNT_PATH);
        return RT_EOK;
    }

    LOG_E("SD card mount to '%s' failed!", SD_CARD_MOUNT_PATH);
    return RT_ERROR;
}

/**
 * @brief 卸载板载 SD 卡文件系统。
 *
 * @return RT_EOK 表示卸载成功，RT_ERROR 表示卸载失败。
 */
int onboard_sdcard_unmount(void)
{
    rt_device_t device = rt_device_find("sd0");

    if (device == RT_NULL || dfs_filesystem_get_mounted_path(device) == RT_NULL)
    {
        return RT_EOK;
    }

    if (dfs_unmount(SD_CARD_MOUNT_PATH) == RT_EOK)
    {
        LOG_I("SD card unmount success");
        return RT_EOK;
    }
    LOG_E("SD card unmount failed!");
    return RT_ERROR;
}

static void sd_card_thread_entry(void *param)
{
    static rt_int8_t _sd_card_state = PIN_HIGH;
    rt_int8_t current_state;

    // 初始化SD卡检测引脚
    rt_pin_mode(SD_CARD_DET_PIN, PIN_MODE_INPUT_PULLUP);

    rt_thread_mdelay(500);

    while (1)
    {
        current_state = rt_pin_read(SD_CARD_DET_PIN);
        if ((current_state == PIN_LOW) && (_sd_card_state == PIN_HIGH))
        {
            LOG_I("SD card insert!");
            if (onboard_sdcard_mount() != RT_EOK)
            {
                LOG_E("onboard_sdcard_mount failed!");
            }
        }
        else if ((current_state == PIN_HIGH) && (_sd_card_state == PIN_LOW))
        {
            LOG_I("SD card remove!");
            if (onboard_sdcard_unmount() != RT_EOK)
            {
                LOG_E("onboard_sdcard_unmount failed!");
            }
            stm32_mmcsd_change();
        }
        _sd_card_state = current_state;
        rt_thread_mdelay(200);
    }
}

/**
 * @brief 初始化并启动 SD 卡监测线程。
 *
 * @return RT_EOK 表示启动成功，其他值表示线程初始化或启动失败。
 */
int sd_card_thread_start(void)
{
    rt_err_t result;

    result = rt_thread_init(&sd_card_thread,
                            "sd_card",
                            sd_card_thread_entry,
                            RT_NULL,
                            sd_card_stack,
                            sizeof(sd_card_stack),
                            THREAD_PRIORITY,
                            THREAD_TIMESLICE);
    if (result != RT_EOK)
    {
        return result;
    }

    return rt_thread_startup(&sd_card_thread);
}
INIT_APP_EXPORT(sd_card_thread_start);
