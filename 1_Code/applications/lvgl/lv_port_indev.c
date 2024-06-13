/*
 * Copyright (c) 2006-2022, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2021-10-18     Meco Man     The first version
 */
#include <lvgl.h>
#include <stdbool.h>
#include <rtdevice.h>
#include <board.h>
#include <ui.h>

extern uint8_t is_current_in_keyboard_edit;

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

void lv_port_indev_read_keypad(struct _lv_indev_drv_t *indev_drv, lv_indev_data_t *data)
{
    uint8_t keyPressFlag=false;
    do{
        if (rt_pin_read(PIN_KEY_UP) == PIN_LOW){
            if (is_current_in_keyboard_edit)
            {
                data->key = LV_KEY_UP;
            }
            else
            {
                data->key = LV_KEY_PREV;
            }
            keyPressFlag = true;
            break;
        }
        if (rt_pin_read(PIN_KEY_DOWN) == PIN_LOW){
            if (is_current_in_keyboard_edit)
            {
                data->key = LV_KEY_DOWN;
            }
            else
            {
                data->key = LV_KEY_NEXT;
            }
            keyPressFlag = true;
            break;
        }
        if(rt_pin_read(PIN_KEY_LEFT) == PIN_LOW){
            data->key = LV_KEY_LEFT;
            keyPressFlag = true;
            break;
        }
        if(rt_pin_read(PIN_KEY_RIGHT) == PIN_LOW){
            data->key = LV_KEY_RIGHT;
            keyPressFlag = true;
            break;
        }
        if(rt_pin_read(PIN_KEY_CENTER) == PIN_LOW){
            data->key = LV_KEY_ENTER;
            keyPressFlag = true;
            break;
        }
    }while (0);
    if(keyPressFlag){
        data->state = LV_INDEV_STATE_PRESSED;
    }else{
        data->state = LV_INDEV_STATE_RELEASED;
    }

}

lv_group_t *menu_group = NULL;
lv_group_t *daplink_group = NULL;
lv_group_t *file_explorer_group = NULL;
lv_group_t *offline_download_group = NULL;
lv_group_t *volt_ammeter_group_main = NULL;
lv_group_t *uart_monitor_group_main = NULL;
lv_group_t *pwm_output_group_main = NULL;
lv_group_t *pwm_output_group_keyboard = NULL;
lv_group_t *dac_output_group_main = NULL;
lv_group_t *dac_output_group_keyboard = NULL;

lv_group_t *about_group_main = NULL;

static lv_indev_drv_t indev_drv;
lv_indev_t *indev;



void lv_port_indev_init(void)
{
    menu_group = lv_group_create();

    offline_download_group = lv_group_create();
	volt_ammeter_group_main = lv_group_create();

    //键盘输入外设

    lv_indev_drv_init(&indev_drv);
    indev_drv.type = LV_INDEV_TYPE_KEYPAD;
    indev_drv.read_cb = lv_port_indev_read_keypad;

    indev = lv_indev_drv_register(&indev_drv);

    lv_group_set_editing(menu_group, false);   //导航模式
    lv_group_add_obj(menu_group,ui_menuDAPLINKB);
    lv_group_add_obj(menu_group,ui_menuOfflineB);
    lv_group_add_obj(menu_group,ui_menuAmmeterB);
    lv_group_add_obj(menu_group,ui_menuUARTB);
    lv_group_add_obj(menu_group,ui_menuPWMB);
    lv_group_add_obj(menu_group,ui_menuDACOutputB);
    lv_group_add_obj(menu_group,ui_menuSDCardB);
	lv_group_add_obj(menu_group,ui_menuAboutB);
    lv_group_focus_obj(ui_menuDAPLINKB);


    lv_indev_set_group(indev, menu_group);
}
