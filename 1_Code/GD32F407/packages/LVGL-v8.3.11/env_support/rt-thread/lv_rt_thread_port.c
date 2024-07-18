/*
 * Copyright (c) 2006-2021, RT-Thread Development Team
 *
 * SPDX-License-Identifier: MIT
 *
 * Change Logs:
 * Date           Author       Notes
 * 2021-10-18     Meco Man     the first version
 * 2022-05-10     Meco Man     improve rt-thread initialization process
 */

#ifdef __RTTHREAD__

#include <lvgl.h>
#include <rtthread.h>
#include <ui.h>
#include <dap_main.h>
#include "lvgl_data_update.h"
#include "screens.h"
#include "lcd_st7789.h"

#define DBG_TAG    "LVGL"
#define DBG_LVL    DBG_INFO
#include <rtdbg.h>

#ifndef PKG_LVGL_THREAD_STACK_SIZE
#define PKG_LVGL_THREAD_STACK_SIZE 4096
#endif /* PKG_LVGL_THREAD_STACK_SIZE */

#ifndef PKG_LVGL_THREAD_PRIO
#define PKG_LVGL_THREAD_PRIO (RT_THREAD_PRIORITY_MAX*2/3)
#endif /* PKG_LVGL_THREAD_PRIO */

extern void lv_port_disp_init(void);
extern void lv_port_indev_init(void);
extern void lv_user_gui_init(void);

static struct rt_thread lvgl_thread;

#ifdef rt_align
rt_align(RT_ALIGN_SIZE)
#else
ALIGN(RT_ALIGN_SIZE)
#endif
__attribute__((section (".TCM"))) static rt_uint8_t lvgl_thread_stack[PKG_LVGL_THREAD_STACK_SIZE];
//static rt_uint8_t lvgl_thread_stack[PKG_LVGL_THREAD_STACK_SIZE];

#if LV_USE_LOG
static void lv_rt_log(const char *buf)
{
    LOG_I(buf);
}
#endif /* LV_USE_LOG */


void LVGL_CentralButton(void)
{
    lv_obj_t *btn = lv_btn_create(lv_scr_act());
    lv_obj_align(btn, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_height(btn, 30);
 
    lv_obj_t *label;
    label = lv_label_create(btn);
    lv_obj_align(label, LV_ALIGN_CENTER, 0, 0);
    lv_label_set_text(label, "LCKFB");
 
    static lv_style_t style_btn;
    lv_style_init(&style_btn);
    lv_style_set_radius(&style_btn, 10);
    lv_style_set_border_color(&style_btn, lv_color_white());
    lv_style_set_border_opa(&style_btn, LV_OPA_30);
    lv_obj_add_style(btn, &style_btn, LV_STATE_DEFAULT);
}

lv_obj_t *img;

void lvgl_load_img(void)
{
//    static lv_obj_t * screen;
//    screen = lv_obj_create(NULL);
//    lv_obj_clear_flag(screen, LV_OBJ_FLAG_SCROLLABLE);      /// Flags

    img = lv_img_create(lv_scr_act());
    lv_img_set_src(img,"S:./images/lckfb2.bmp");
    lv_obj_set_width(img, 240);   /// 200
    lv_obj_set_height(img, 240);    /// 47
    lv_obj_align(img, LV_ALIGN_CENTER, 0, 0);

//	lv_obj_t* list = lv_list_create(lv_scr_act());
//    lv_obj_set_size(list, 240, 30);
//    lv_obj_align(list, LV_ALIGN_CENTER, 0, 0);

//    lv_list_add_text(list,"lckfb");
//    lv_obj_t  *btnWifi = lv_list_add_btn(list,LV_SYMBOL_WIFI,"wifi");
    // lv_group_add_obj(group1,btnWifi);

//    lv_obj_t *btn = lv_btn_create(lv_scr_act());
//    lv_obj_align(btn, LV_ALIGN_CENTER, 0, 0);
//    lv_obj_set_height(btn, 30);
//    
//    lv_obj_t *label;
//    label = lv_label_create(btn);
//    lv_obj_align(label, LV_ALIGN_CENTER, 0, 0);
//    lv_label_set_text(label, "LCKFB");

//    static lv_style_t style_btn;
//    lv_style_init(&style_btn);
//    lv_style_set_radius(&style_btn, 10);
//    lv_style_set_border_color(&style_btn, lv_color_white());
//    lv_style_set_border_opa(&style_btn, LV_OPA_30);
//    lv_obj_add_style(btn, &style_btn, LV_STATE_DEFAULT);

//    lv_scr_load(screen);
}

static void lvgl_thread_entry(void *parameter)
{
#if LV_USE_LOG
    lv_log_register_print_cb(lv_rt_log);
#endif /* LV_USE_LOG */

	rt_device_t lcd = rt_device_find("lcd");
	rt_device_open(lcd, RT_DEVICE_FLAG_RDWR);

	LCD_Clear(0xFFFF);

    LCD_BLK_SET;        /* Open Backlight */
//	
//	LCD_Fill(120, 60, 120,60,0x07E0);
	
    lv_init();
    lv_port_disp_init();
//	LVGL_CentralButton();
//	
//extern void lv_demo_benchmark(void);
//	lv_demo_benchmark();
//    lv_user_gui_init();

//    rt_thread_mdelay(500);
//    lvgl_load_img();

extern void ui_init(void);
	ui_init();

    lv_port_indev_init();
uint32_t temp_count= 0;
    /* handle the tasks of LVGL */
    while(1)
    {
        lv_task_handler();
        rt_thread_mdelay(LV_DISP_DEF_REFR_PERIOD);
		temp_count++;
		if(temp_count >= 10)
		{
			temp_count = 0;
		}

        if (current_screen_get() == SCREEN_DAPLINK)
        {
            update_daplink_uart_data();
            update_daplink_dbg_data();
            update_daplink_voltage_current_data();
            update_daplink_idcode();
        }
        else if (current_screen_get() == SCREEN_OFFLINE_DOWNLOAD)
        {
            update_offline_downlaod_info();
        }
        else if (current_screen_get() == SCREEN_VOLT_AMMETER)
        {
            update_volt_ammeter_voltage_current_data();
			update_volt_ammeter_pd_sink_state();
        }
        else if(current_screen_get() == SCREEN_UART_MONITOR)
        {
            update_uart_monitor_text_area();
        }else if(current_screen_get() == SCREEN_PWM_OUTPUT)
        {
            update_pwm_frequency_and_duty();
        }
    }
}

static int lvgl_thread_init(void)
{
    rt_err_t err;

    err = rt_thread_init(&lvgl_thread, "LVGL", lvgl_thread_entry, RT_NULL,
           &lvgl_thread_stack[0], sizeof(lvgl_thread_stack), PKG_LVGL_THREAD_PRIO, 10);
    if(err != RT_EOK)
    {
        LOG_E("Failed to create LVGL thread");
        return -1;
    }
    rt_thread_startup(&lvgl_thread);

    return 0;
}
INIT_ENV_EXPORT(lvgl_thread_init);

#endif /*__RTTHREAD__*/
