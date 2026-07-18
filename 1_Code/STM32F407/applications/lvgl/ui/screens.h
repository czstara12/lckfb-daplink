/*
* 梁山派软硬件资料与相关扩展板软硬件资料官网全部开源
* 开发板官网：www.lckfb.com
* 技术支持常驻论坛，任何技术问题欢迎随时交流学习
* 立创论坛：club.szlcsc.com
* 关注bilibili账号：【立创开发板】，掌握我们的最新动态！
* 不靠卖板赚钱，以培养中国工程师为己任
* Change Logs:
* Date           Author       Notes
* 2024-05-10     LCKFB-yzh    first version
 */

#ifndef __SCREEN_H__
#define __SCREEN_H__

#include "board.h"
#include "rtthread.h"

enum CURRENT_SCREEN {
    SCREEN_MENU = 1,
    SCREEN_DAPLINK,
    SCREEN_OFFLINE_DOWNLOAD,
    SCREEN_VOLT_AMMETER,
    SCREEN_FILE_EXPLORER,
    SCREEN_UART_MONITOR,
    SCREEN_PWM_OUTPUT,
    SCREEN_DAC_OUTPUT,
    SCREEN_ABOUT
};

void current_screen_set(enum CURRENT_SCREEN screen);
enum CURRENT_SCREEN current_screen_get(void);

#endif /* __SCREEN_H__ */
