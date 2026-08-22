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
#include <stdlib.h>
#include <unistd.h>
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
#define SD_SPEED_PATH              SD_CARD_MOUNT_PATH "/.speed_test.bin"
#define SD_SPEED_DEFAULT_SIZE_KIB  (4U * 1024U)
#define SD_SPEED_DEFAULT_BLOCK     (4U * 1024U)
#define SD_SPEED_MAX_SIZE_KIB      (64U * 1024U)
#define SD_SPEED_MAX_BLOCK         (4U * 1024U)

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

static int sd_speed_parse(const char *text, rt_uint32_t min,
                          rt_uint32_t max, rt_uint32_t *value)
{
    char *end;
    unsigned long parsed = strtoul(text, &end, 0);

    if (*text == '\0' || *end != '\0' || parsed < min || parsed > max)
    {
        return -RT_EINVAL;
    }
    *value = (rt_uint32_t)parsed;
    return RT_EOK;
}

static void sd_speed_print(const char *operation, rt_uint32_t size_kib,
                           rt_tick_t ticks)
{
    rt_uint32_t elapsed_ms;
    rt_uint32_t speed_kib_s;

    if (ticks == 0U)
    {
        ticks = 1U;
    }
    elapsed_ms = (rt_uint32_t)(((rt_uint64_t)ticks * 1000U) / RT_TICK_PER_SECOND);
    speed_kib_s = (rt_uint32_t)(((rt_uint64_t)size_kib * RT_TICK_PER_SECOND) / ticks);
    rt_kprintf("%s: %u KiB, %u ms, %u KiB/s\n",
               operation, size_kib, elapsed_ms, speed_kib_s);
}

/**
 * @brief 测试 SD 卡文件系统顺序写入和读取速度，并校验读取内容。
 *
 * @param argc 参数数量。
 * @param argv 参数列表：sd_speed [size_kib] [block_bytes]。
 */
static void sd_speed(int argc, char **argv)
{
    rt_uint32_t size_kib = SD_SPEED_DEFAULT_SIZE_KIB;
    rt_uint32_t block_size = SD_SPEED_DEFAULT_BLOCK;
    rt_uint32_t total_size;
    rt_uint32_t done;
    rt_uint8_t *buffer;
    rt_tick_t start_tick;
    rt_tick_t write_ticks;
    rt_tick_t read_ticks;
    int fd = -1;

    if (argc > 3 ||
        (argc > 1 && sd_speed_parse(argv[1], 1U, SD_SPEED_MAX_SIZE_KIB,
                                   &size_kib) != RT_EOK) ||
        (argc > 2 && sd_speed_parse(argv[2], 1U, SD_SPEED_MAX_BLOCK,
                                   &block_size) != RT_EOK))
    {
        rt_kprintf("usage: sd_speed [size_kib:1..65536] [block_bytes:1..4096]\n");
        return;
    }

    total_size = size_kib * 1024U;
    if (block_size > total_size)
    {
        block_size = total_size;
    }
    buffer = rt_malloc(block_size);
    if (buffer == RT_NULL)
    {
        rt_kprintf("sd_speed: allocate %u bytes failed\n", block_size);
        return;
    }
    rt_memset(buffer, 0xA5, block_size);
    unlink(SD_SPEED_PATH);

    start_tick = rt_tick_get();
    fd = open(SD_SPEED_PATH, O_WRONLY | O_CREAT | O_TRUNC, 0);
    for (done = 0U; fd >= 0 && done < total_size;)
    {
        rt_uint32_t chunk = total_size - done > block_size
                                ? block_size
                                : total_size - done;
        int written = write(fd, buffer, chunk);

        if (written <= 0)
        {
            break;
        }
        done += (rt_uint32_t)written;
    }
    if (fd < 0 || done != total_size || fsync(fd) != 0 || close(fd) != 0)
    {
        rt_kprintf("sd_speed: write failed at %u/%u bytes\n", done, total_size);
        goto cleanup;
    }
    fd = -1;
    write_ticks = rt_tick_get() - start_tick;

    rt_memset(buffer, 0, block_size);
    start_tick = rt_tick_get();
    fd = open(SD_SPEED_PATH, O_RDONLY, 0);
    for (done = 0U; fd >= 0 && done < total_size;)
    {
        rt_uint32_t chunk = total_size - done > block_size
                                ? block_size
                                : total_size - done;
        int read_size = read(fd, buffer, chunk);
        rt_uint32_t index;

        if (read_size <= 0)
        {
            break;
        }
        for (index = 0U; index < (rt_uint32_t)read_size; index++)
        {
            if (buffer[index] != 0xA5U)
            {
                rt_kprintf("sd_speed: verify failed at byte %u\n", done + index);
                goto cleanup;
            }
        }
        done += (rt_uint32_t)read_size;
    }
    if (fd < 0 || done != total_size || close(fd) != 0)
    {
        rt_kprintf("sd_speed: read failed at %u/%u bytes\n", done, total_size);
        goto cleanup;
    }
    fd = -1;
    read_ticks = rt_tick_get() - start_tick;

    sd_speed_print("write", size_kib, write_ticks);
    sd_speed_print("read ", size_kib, read_ticks);
    rt_kprintf("verify: passed, block: %u bytes\n", block_size);

cleanup:
    if (fd >= 0)
    {
        close(fd);
    }
    unlink(SD_SPEED_PATH);
    rt_free(buffer);
}
MSH_CMD_EXPORT(sd_speed, test SD card speed: sd_speed [size_kib] [block_bytes]);
