/*
* 梁山派软硬件资料与相关扩展板软硬件资料官网全部开源
* 开发板官网：www.lckfb.com
* 技术支持常驻论坛，任何技术问题欢迎随时交流学习
* 立创论坛：club.szlcsc.com
* 关注bilibili账号：【立创开发板】，掌握我们的最新动态！
* 不靠卖板赚钱，以培养中国工程师为己任
* Change Logs:
* Date           Author       Notes
* 2024-06-05     LCKFB-yzh    first version
*/

#include "bsp_led.h"

static GPIO_TypeDef *LED_PORT[LED_MAX_NUM] = {LED1_GPIO_PORT, LED2_GPIO_PORT,
                                              LED3_GPIO_PORT, LED4_GPIO_PORT};

static uint32_t LED_PIN[LED_MAX_NUM] = {LED1_PIN, LED2_PIN, LED3_PIN, LED4_PIN};
/**
  -  @brief  初始化LED灯
  -  @note   None
  -  @param  led：LED1,LED2,LED3,LED4
  -  @retval None
 */
static void bsp_led_gpio_init(led_type_def led)
{
   GPIO_InitTypeDef GPIO_InitStruct = {0};

    /* configure led GPIO port */
    GPIO_InitStruct.Pin = LED_PIN[led];
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(LED_PORT[led], &GPIO_InitStruct);
}

 /**
  -  @brief  初始化所有LED灯，并设置LED为关闭状态
  -  @note   None
  -  @param  None
  -  @retval None
 */
void bsp_led_init(void)
{
    /* enable the led clock */
    LED1_GPIO_CLK_EN();
    LED2_GPIO_CLK_EN();
    LED3_GPIO_CLK_EN();
    LED4_GPIO_CLK_EN();
    for (led_type_def _led = LED1; _led < LED_MAX_NUM; _led++)
    {
        bsp_led_gpio_init(_led);
        bsp_led_off(_led);
    }
}
 /**
  -  @brief  点亮led
  -  @note   None
  -  @param  led：LED1,LED2,LED3,LED4
  -  @retval None
 */
void bsp_led_on(led_type_def led)
{
    HAL_GPIO_WritePin(LED_PORT[led], LED_PIN[led],GPIO_PIN_RESET);
}

/**
 -  @brief  熄灭led
 -  @note   None
 -  @param  led：LED1,LED2,LED3,LED4
 -  @retval None
*/
void bsp_led_off(led_type_def led)
{
  HAL_GPIO_WritePin(LED_PORT[led], LED_PIN[led],GPIO_PIN_SET);
}

/**
 -  @brief  翻转led
 -  @note   None
 -  @param  led：LED1,LED2,LED3,LED4
 -  @retval None
*/
void bsp_led_toggle(led_type_def led)
{
  HAL_GPIO_TogglePin(LED_PORT[led], LED_PIN[led]);
}

/**
 -  @brief  获取LED状态
 -  @note   None
 -  @param  None
 -  @retval 0：LED灯熄灭，1：LED灯亮了
*/
uint8_t bsp_led_get_status(led_type_def led)
{
    GPIO_PinState led_status = GPIO_PIN_SET;
    led_status = HAL_GPIO_ReadPin(LED_PORT[led], LED_PIN[led]);
    if (led_status == GPIO_PIN_RESET)
    {
        return 0;
    }
    else
    {
        return 1;
    }
}

/**
-  @brief  让天空星多功能调试器上的led来回闪烁，每调用一次就切换成下一个灯亮
 -  @note   None
 -  @param  None
 -  @retval None
*/
void bsp_led_left_right_move(void) {
  static int8_t led_direction = 0;
  static led_type_def _led_pos = LED1;
  if (led_direction == 0) {
    if (_led_pos != LED1) {
      bsp_led_off((led_type_def)(_led_pos - 1));
    }
    bsp_led_on(_led_pos);
    if (_led_pos >= LED4) {
      led_direction = 1;
    }
    _led_pos++;
  } else {
    _led_pos--;
    if (_led_pos != LED4) {
      bsp_led_off((led_type_def)(_led_pos + 1));
    }
    bsp_led_on(_led_pos);
    if (_led_pos <= LED1) {
      led_direction = 0;
    }
  }
}
