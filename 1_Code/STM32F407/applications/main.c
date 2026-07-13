/*
 * Copyright (c) 2023, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2023-07-06     Supperthomas first version
 * 2023-12-03     Meco Man     support nano version
 * 2024-04-13     yuanzihao    adaptation for SkyStar STM32F407 version
 */

#include <board.h>
#include <rtthread.h>
#include <drv_gpio.h>
#ifndef RT_USING_NANO
#include <rtdevice.h>
#endif /* RT_USING_NANO */
#include <drv_common.h>
#include "bsp_led.h"
#include "runtime_load_indicator.h"

#define GPIO_LED    GET_PIN(B, 2)

/**
 * @brief 挂载板载文件系统。
 *
 * @return 成功返回 RT_EOK。
 */
extern int filesystem_mount(void);

static void runtime_load_bar_update(void)
{
    rt_uint8_t load = runtime_load_indicator_get_load();
    rt_uint8_t level = (load + 24U) / 25U;

    if (level > LED_MAX_NUM)
    {
        level = LED_MAX_NUM;
    }

    for (led_type_def led = LED1; led < LED_MAX_NUM; led++)
    {
        if ((rt_uint8_t)led < level)
        {
            bsp_led_on(led);
        }
        else
        {
            bsp_led_off(led);
        }
    }
}

int main(void)
{
    rt_pin_mode(GPIO_LED, PIN_MODE_OUTPUT);
    bsp_led_init();
    runtime_load_indicator_start();
    filesystem_mount();

    while (1)
    {
        runtime_load_indicator_process();
        runtime_load_bar_update();
        rt_thread_mdelay(100);
    }
}
