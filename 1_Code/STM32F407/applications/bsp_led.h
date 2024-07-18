/*
* 梁山派软硬件资料与相关扩展板软硬件资料官网全部开源
* 开发板官网：www.lckfb.com
* 技术支持常驻论坛，任何技术问题欢迎随时交流学习
* 立创论坛：club.szlcsc.com
* 关注bilibili账号：【立创开发板】，掌握我们的最新动态！
* 不靠卖板赚钱，以培养中国工程师为己任
* Change Logs:
* Date           Author       Notes
* 2023-07-21     LCKFB-yzh    first version
*/

#ifndef __BSP_LED_H__
#define __BSP_LED_H__

#include "main.h"
#include "stm32f4xx.h"

typedef enum
{
    LED1 = 0,
    LED2 = 1,
    LED3 = 2,
    LED4 = 3,
    LED_MAX_NUM
} led_type_def;

#define LED1_PIN                         GPIO_PIN_13
#define LED1_GPIO_PORT                   GPIOC
#define LED1_GPIO_CLK_EN()               __HAL_RCC_GPIOC_CLK_ENABLE()

#define LED2_PIN                         GPIO_PIN_5
#define LED2_GPIO_PORT                   GPIOE
#define LED2_GPIO_CLK_EN()               __HAL_RCC_GPIOE_CLK_ENABLE()

#define LED3_PIN                         GPIO_PIN_3
#define LED3_GPIO_PORT                   GPIOE
#define LED3_GPIO_CLK_EN()               __HAL_RCC_GPIOE_CLK_ENABLE()

#define LED4_PIN                         GPIO_PIN_6
#define LED4_GPIO_PORT                   GPIOE
#define LED4_GPIO_CLK_EN()               __HAL_RCC_GPIOE_CLK_ENABLE()


void bsp_led_init(void);
void bsp_led_on(led_type_def led);
void bsp_led_off(led_type_def led);
void bsp_led_toggle(led_type_def led);
uint8_t bsp_led_get_status(led_type_def led);
void bsp_led_left_right_move(void);

#endif /* __BSP_LED_H__ */
