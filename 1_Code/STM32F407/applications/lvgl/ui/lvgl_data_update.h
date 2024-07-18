/*
* 梁山派软硬件资料与相关扩展板软硬件资料官网全部开源
* 开发板官网：www.lckfb.com
* 技术支持常驻论坛，任何技术问题欢迎随时交流学习
* 立创论坛：club.szlcsc.com
* 关注bilibili账号：【立创开发板】，掌握我们的最新动态！
* 不靠卖板赚钱，以培养中国工程师为己任
* Change Logs:
* Date           Author       Notes
* 2024-04-11     LCKFB-yzh    first version
*/

#ifndef LVGL_DATA_UPDATE_H
#define LVGL_DATA_UPDATE_H

#include "board.h"
#include <dap_main.h>

#define CONFIG_UARTRX_RINGBUF_SIZE_FOR_LVGL (256)

void set_daplink_uart_is_need_update(void);

void reset_daplink_uart_is_need_update(void);

void update_daplink_uart_data(void);
void update_daplink_dbg_data(void);
void update_offline_downlaod_info(void);
void update_daplink_voltage_current_data(void);
void update_daplink_idcode(void);
void update_volt_ammeter_voltage_current_data(void);
void update_volt_ammeter_pd_sink_state(void);
void update_uart_monitor_text_area(void);
void update_pwm_frequency_and_duty(void);

#endif /* LVGL_DATA_UPDATE_H */
