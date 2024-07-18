/**
 * @File:    demo_rtt_iotboard.c
 * @Author:  MurphyZhao
 * @Date:    2018-09-29
 *
 * Copyright (c) 2018-2019 MurphyZhao <d2014zjt@163.com>
 *               https://github.com/murphyzhao
 * All rights reserved.
 * License-Identifier: Apache-2.0
 *
 * Licensed under the Apache License, Version 2.0 (the "License"); you may
 * not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *    http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * message:
 * This demo is base on rt-thread IoT Board, reference
 *     https://github.com/RT-Thread/IoT_Board
 * Hardware version: RT-Thread IoT Board Pandora v2.51.
 *
 * Change logs:
 * Date        Author       Notes
 * 2018-09-29  MurphyZhao   First add
 * 2019-08-02  MurphyZhao   Migrate code to github.com/murphyzhao account
 * 2020-02-14  MurphyZhao   Fix grammar bug
 * 2024-03-15  LCKFB-yzh    适配DAPLINK按键
*/

#include <rtthread.h>
#include <board.h>

#include "flexible_button.h"

#ifndef PIN_KEY_CENTER
#define PIN_KEY_CENTER GET_PIN(A, 15)
#endif

#ifndef PIN_KEY_RIGHT
#define PIN_KEY_RIGHT GET_PIN(D, 1)
#endif

#ifndef PIN_KEY_LEFT
#define PIN_KEY_LEFT GET_PIN(C, 7)
#endif

#ifndef PIN_KEY_UP
#define PIN_KEY_UP GET_PIN(A, 8)
#endif

#ifndef PIN_KEY_DOWN
#define PIN_KEY_DOWN GET_PIN(D, 0)
#endif

#define ENUM_TO_STR(e) (#e)

typedef enum
{
    USER_BUTTON_KEY_CENTER = 0,
    USER_BUTTON_KEY_UP,
    USER_BUTTON_KEY_DOWN,
    USER_BUTTON_KEY_LEFT,
    USER_BUTTON_KEY_RIGHT,
    USER_BUTTON_MAX
} user_button_t;

static char *enum_event_string[] = {
    ENUM_TO_STR(FLEX_BTN_PRESS_DOWN),
    ENUM_TO_STR(FLEX_BTN_PRESS_CLICK),
    ENUM_TO_STR(FLEX_BTN_PRESS_DOUBLE_CLICK),
    ENUM_TO_STR(FLEX_BTN_PRESS_REPEAT_CLICK),
    ENUM_TO_STR(FLEX_BTN_PRESS_SHORT_START),
    ENUM_TO_STR(FLEX_BTN_PRESS_SHORT_UP),
    ENUM_TO_STR(FLEX_BTN_PRESS_LONG_START),
    ENUM_TO_STR(FLEX_BTN_PRESS_LONG_UP),
    ENUM_TO_STR(FLEX_BTN_PRESS_LONG_HOLD),
    ENUM_TO_STR(FLEX_BTN_PRESS_LONG_HOLD_UP),
    ENUM_TO_STR(FLEX_BTN_PRESS_MAX),
    ENUM_TO_STR(FLEX_BTN_PRESS_NONE),
};

static char *enum_btn_id_string[] = {
    ENUM_TO_STR(USER_BUTTON_KEY_CENTER),
    ENUM_TO_STR(USER_BUTTON_KEY_UP),
    ENUM_TO_STR(USER_BUTTON_KEY_DOWN),
    ENUM_TO_STR(USER_BUTTON_KEY_LEFT),
    ENUM_TO_STR(USER_BUTTON_KEY_RIGHT),
    ENUM_TO_STR(USER_BUTTON_MAX),
};

static flex_button_t user_button[USER_BUTTON_MAX];

static uint8_t common_btn_read(void *arg)
{
    uint8_t value = 0;

    flex_button_t *btn = (flex_button_t *)arg;

    switch (btn->id)
    {
    case USER_BUTTON_KEY_CENTER:
        value = rt_pin_read(PIN_KEY_CENTER);
        break;
    case USER_BUTTON_KEY_UP:
        value = rt_pin_read(PIN_KEY_UP);
        break;
    case USER_BUTTON_KEY_DOWN:
        value = rt_pin_read(PIN_KEY_DOWN);
        break;
    case USER_BUTTON_KEY_LEFT:
        value = rt_pin_read(PIN_KEY_LEFT);
        break;
    case USER_BUTTON_KEY_RIGHT:
        value = rt_pin_read(PIN_KEY_RIGHT);
        break;
    default:
        RT_ASSERT(0);
    }

    return value;
}


//#include "SWD_flash.h"
//#include "swd_host.h"
//#include "debug_cm.h"

//#include "applications/flash_algo/STM32F4xx_512.c"
//uint32_t Flash_Sect_Size = 1024;
//uint32_t Flash_Page_Size = 1024;
//uint32_t Flash_Start_Addr = 0x08000000;


//#include "applications/flash_algo/f407_100ms.c"
//#define target_bin1 f407_100ms

//#include "applications/flash_algo/f407_500ms.c"
//#define target_bin2 f407_500ms

//uint8_t buff[1024] = {0};

//extern void soft_reset_target(void);

//void temp_download_to_mcu_100ms(void)
//{
//    uint32_t val;
//    uint32_t i;

//    swd_init_debug();

//	swd_set_target_state_hw(RESET_PROGRAM);
//	
//    swd_read_dp(0x00, &val);
//    rt_kprintf("\n\nIDCODE: %08X\n\n", val);

//    target_flash_init(Flash_Start_Addr);
//    rt_kprintf("1\n\n\n");

//    for (uint32_t addr = 0; addr < sizeof(target_bin1); addr += Flash_Sect_Size)
//    {
//        target_flash_erase_sector(Flash_Start_Addr + addr);
//    }

//    rt_kprintf("2\n\n\n");
//    for (uint32_t addr = 0; addr < sizeof(target_bin1); addr += 1024)
//    {
//        swd_read_memory(Flash_Start_Addr + addr, buff, 1024);
//        // for (uint32_t i = 0; i < 1024; i++)
//        //     rt_kprintf("%02X ", buff[i]);
//        // rt_kprintf("\n\n\n");
//    }
//    rt_kprintf("3\n\n\n");
//    for (uint32_t addr = 0; addr < sizeof(target_bin1); addr += Flash_Page_Size)
//    {
//        target_flash_program_page(Flash_Start_Addr + addr, &target_bin1[addr],
//                                  1024);
//    }
//    rt_kprintf("4\n\n\n");
//    for (uint32_t addr = 0; addr < sizeof(target_bin1); addr += 1024)
//    {
//        swd_read_memory(Flash_Start_Addr + addr, buff, 1024);
//        // for (uint32_t i = 0; i < 1024; i++)
//            // rt_kprintf("%02X ", buff[i]);

//        if (memcmp(&target_bin1[addr], buff,
//                   sizeof(target_bin1) - addr > 1024
//                       ? 1024
//                       : sizeof(target_bin1) - addr)
//            == 0)
//            rt_kprintf("\nPass\n\n\n");
//        else
//            rt_kprintf("\nFail\n\n\n");
//    }

//    soft_reset_target();
//}

//void temp_download_to_mcu_500ms(void)
//{
//    uint32_t val;

//    swd_init_debug();

//    swd_read_dp(0x00, &val);
//    rt_kprintf("\n\nIDCODE: %08X\n\n", val);

//    target_flash_init(Flash_Start_Addr);

//    for (uint32_t addr = 0; addr < sizeof(target_bin2); addr += Flash_Sect_Size)
//    {
//        target_flash_erase_sector(Flash_Start_Addr + addr);
//    }

//    for (uint32_t addr = 0; addr < sizeof(target_bin2); addr += 1024)
//    {
//        swd_read_memory(Flash_Start_Addr + addr, buff, 1024);
//        for (uint32_t i = 0; i < 1024; i++)
//            rt_kprintf("%02X ", buff[i]);
//        rt_kprintf("\n\n\n");
//    }

//    for (uint32_t addr = 0; addr < sizeof(target_bin2); addr += Flash_Page_Size)
//    {
//        target_flash_program_page(Flash_Start_Addr + addr, &target_bin2[addr],
//                                  1024);
//    }

//    for (uint32_t addr = 0; addr < sizeof(target_bin2); addr += 1024)
//    {
//        swd_read_memory(Flash_Start_Addr + addr, buff, 1024);
//        for (uint32_t i = 0; i < 1024; i++)
//            rt_kprintf("%02X ", buff[i]);

//        if (memcmp(&target_bin2[addr], buff,
//                   sizeof(target_bin2) - addr > 1024
//                       ? 1024
//                       : sizeof(target_bin2) - addr)
//            == 0)
//            rt_kprintf("\nPass\n\n\n");
//        else
//            rt_kprintf("\nFail\n\n\n");
//    }
//    soft_reset_target();
//}

extern void swd_download_100ms(void);
extern void swd_download_500ms(void);


static void common_btn_evt_cb(void *arg)
{
    flex_button_t *btn = (flex_button_t *)arg;

    rt_kprintf("id: [%d - %s]  event: [%d - %30s]  repeat: %d\n",
        btn->id, enum_btn_id_string[btn->id],
        btn->event, enum_event_string[btn->event],
        btn->click_cnt);

    // if (flex_button_event_read(&user_button[USER_BUTTON_KEY_LEFT]) == FLEX_BTN_PRESS_CLICK)
    // {
    //     rt_kprintf("start to download stm32f407vet6 led 100ms blink\n");
    //     swd_download_100ms();
    // }
    // if (flex_button_event_read(&user_button[USER_BUTTON_KEY_RIGHT]) == FLEX_BTN_PRESS_CLICK)
    // {
    //     rt_kprintf("start to download stm32f407vet6 led 500ms blink\n");
    //     swd_download_500ms();
    // }
}

static void button_scan(void *arg)
{
    while(1)
    {
        flex_button_scan();
        rt_thread_mdelay(20); // 20 ms
    }
}

static void user_button_init(void)
{
    int i;

    rt_memset(&user_button[0], 0x0, sizeof(user_button));

    rt_pin_mode(PIN_KEY_CENTER, PIN_MODE_INPUT_PULLUP); /* set KEY pin mode to input */
    rt_pin_mode(PIN_KEY_UP, PIN_MODE_INPUT_PULLUP); /* set KEY pin mode to input */
    rt_pin_mode(PIN_KEY_DOWN, PIN_MODE_INPUT_PULLUP); /* set KEY pin mode to input */
    rt_pin_mode(PIN_KEY_LEFT, PIN_MODE_INPUT_PULLUP); /* set KEY pin mode to input */
    rt_pin_mode(PIN_KEY_RIGHT, PIN_MODE_INPUT_PULLUP); /* set KEY pin mode to input */

    for (i = 0; i < USER_BUTTON_MAX; i ++)
    {
        user_button[i].id = i;
        user_button[i].usr_button_read = common_btn_read;
        user_button[i].cb = common_btn_evt_cb;
        user_button[i].pressed_logic_level = 0;
        user_button[i].short_press_start_tick = FLEX_MS_TO_SCAN_CNT(1500);
        user_button[i].long_press_start_tick = FLEX_MS_TO_SCAN_CNT(3000);
        user_button[i].long_hold_start_tick = FLEX_MS_TO_SCAN_CNT(4500);

        flex_button_register(&user_button[i]);
    }
}

int flex_button_main(void)
{
    rt_thread_t tid = RT_NULL;

    user_button_init();

    /* Create background ticks thread */
    tid = rt_thread_create("flex_btn", button_scan, RT_NULL, 4096, 10, 10);
    if(tid != RT_NULL)
    {
        rt_thread_startup(tid);
    }

    return 0;
}
#ifdef FINSH_USING_MSH
INIT_APP_EXPORT(flex_button_main);
#endif
