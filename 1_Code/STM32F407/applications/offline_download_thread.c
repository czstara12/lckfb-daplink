/*
 * Copyright (c) 2006-2024, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2024-04-23     yuanzihao    first implementation
 */

#include <stdio.h>
#include <rtthread.h>
#include <rtdevice.h>
#include <board.h>

#include "dap_main.h"
#include "dap.h"
#include "ui.h"
#include "bsp_beep.h"
#include "swd_download_file.h"

#define LOG_TAG     "offline download"     // 该模块对应的标签。不定义时，默认：NO_TAG
#define LOG_LVL     LOG_LVL_DBG   // 该模块对应的日志输出级别。不定义时，默认：调试级别
#include <ulog.h>                 // 必须在 LOG_TAG 与 LOG_LVL 下面

#define OFFLINE_DOWNLOAD_PRIORITY         21
#define OFFLINE_DOWNLOAD_TIMESLICE        5


rt_align(RT_ALIGN_SIZE)
static char __ALIGN_BEGIN offline_download_stack[4096];

static struct rt_thread offline_download;


extern char choose_device_path[LV_FILE_EXPLORER_PATH_MAX_LEN];
extern char choose_firmware_bin_path[LV_FILE_EXPLORER_PATH_MAX_LEN];

/* 指向互斥量的指针 */
rt_sem_t offline_download_sem = RT_NULL;


/* DAPLINK 线程 入口 */
static void offline_download_entry(void *param)
{
    offline_download_sem = rt_sem_create("offline_download_sem", 0,RT_IPC_FLAG_FIFO);//默认没有信号量
    if(offline_download_sem == RT_NULL)
    {
        rt_kprintf("offline_download_mutex create failed!\n");
        return ;
    }
    while (1)
    {
        rt_sem_take(offline_download_sem, RT_WAITING_FOREVER);
        LOG_D("offline download start!");
        LOG_D("device_path:%s", choose_device_path);
        LOG_D("firmware_bin_path:%s", choose_firmware_bin_path);
		rt_thread_mdelay(100);
        if (swd_download_update_flash_algo(choose_device_path) < 0 ||
            swd_download_from_file(choose_firmware_bin_path)
            == -1)
        {
            // 下载错误
            buzzer_beep_set(4000, 100);
            rt_thread_mdelay(50);
            buzzer_beep_set(0, 0);
            rt_thread_mdelay(50);
            buzzer_beep_set(4000, 100);
            rt_thread_mdelay(50);
            buzzer_beep_set(0, 0);
            rt_thread_mdelay(50);
            buzzer_beep_set(4000, 100);
            rt_thread_mdelay(50);
            buzzer_beep_set(0, 0);
            rt_thread_mdelay(50);
        }
        else
        {
            // 下载成功
            buzzer_beep_set(4000, 100);
            rt_thread_mdelay(50);
            buzzer_beep_set(0, 0);
            rt_thread_mdelay(50);
        }
    }
}

/* 启动DAPLINK线程 */
int offline_download_startup(void)
{
  rt_thread_init(&offline_download,
                 "offline_download",
                 offline_download_entry,
                 RT_NULL,
                 &offline_download_stack[0],
                 sizeof(offline_download_stack),
                   OFFLINE_DOWNLOAD_PRIORITY - 1, OFFLINE_DOWNLOAD_TIMESLICE);
  rt_thread_startup(&offline_download);

  return 0;
}

INIT_APP_EXPORT(offline_download_startup);
