/*
* 梁山派软硬件资料与相关扩展板软硬件资料官网全部开源
* 开发板官网：www.lckfb.com
* 技术支持常驻论坛，任何技术问题欢迎随时交流学习
* 立创论坛：club.szlcsc.com
* 关注bilibili账号：【立创开发板】，掌握我们的最新动态！
* 不靠卖板赚钱，以培养中国工程师为己任
* Change Logs:
* Date           Author       Notes
* 2024-02-22     LCKFB-yzh    first version
*/
#include <stdio.h>
#include <board.h>
#include <bsp_beep.h>

/**
 -  @brief  蜂鸣器PWM引脚初始化
 -  @note   None
 -  @param  None
 -  @retval None
   */
static void beep_pwm_gpio_init(void)
{
    // servo pwm gpio init
    rcu_periph_clock_enable(BEEP_GPIO_CLK);

    gpio_mode_set(BEEP_GPIO_PORT, GPIO_MODE_AF, BEEP_GPIO_PUPD, BEEP_GPIO_PIN);

    rcu_periph_clock_enable(RCU_SYSCFG);

    gpio_af_set(BEEP_GPIO_PORT, BEEP_GPIO_ALT_FUNC, BEEP_GPIO_PIN);

    gpio_output_options_set(BEEP_GPIO_PORT, GPIO_OTYPE_PP, GPIO_OSPEED_2MHZ,
                            BEEP_GPIO_PIN);
    return;
}

/**
 -  @brief  蜂鸣器定时器初始化
 -  @note   None
 -  @param  None
 -  @retval None
   */
static void beep_timer_init(void)
{
    // beep pwm timer init
    timer_parameter_struct timer_param_type;
    timer_oc_parameter_struct timer_oc_init_struct;

    rcu_periph_clock_enable(BEEP_TIMER_CLK);
    timer_deinit(BEEP_TIMER);
    timer_struct_para_init(&timer_param_type);

    timer_param_type.alignedmode = TIMER_COUNTER_EDGE;
    timer_param_type.clockdivision = TIMER_CKDIV_DIV1;
    timer_param_type.counterdirection = TIMER_COUNTER_UP;
    timer_param_type.period = 250 - 1;   //4000Hz
    timer_param_type.prescaler = 168-1;   //120Mhz/120=1 000 000Hz
    timer_param_type.repetitioncounter = 0;
    timer_init(BEEP_TIMER, &timer_param_type);

    timer_channel_output_struct_para_init(&timer_oc_init_struct);
    timer_oc_init_struct.ocpolarity = TIMER_OC_POLARITY_HIGH;
    timer_oc_init_struct.outputstate = TIMER_CCX_ENABLE;
    timer_oc_init_struct.outputnstate = TIMER_CCXN_DISABLE;
    timer_oc_init_struct.ocnpolarity = TIMER_OCN_POLARITY_HIGH;
    timer_oc_init_struct.ocidlestate = TIMER_OC_IDLE_STATE_LOW;
    timer_oc_init_struct.ocnidlestate = TIMER_OCN_IDLE_STATE_LOW;

    timer_channel_output_config(BEEP_TIMER, TIMER_CH_0, &timer_oc_init_struct);
    timer_channel_output_pulse_value_config(BEEP_TIMER, TIMER_CH_0, 0);
    timer_channel_output_mode_config(BEEP_TIMER,TIMER_CH_0,TIMER_OC_MODE_PWM0);
    timer_channel_output_shadow_config(BEEP_TIMER,TIMER_CH_0,TIMER_OC_SHADOW_DISABLE);

    timer_enable(BEEP_TIMER);

    return;
}

/**
 -  @brief  蜂鸣器的PWM频率和周期设置
 -  @note   None
-  @param  _tone_freq:PWM频率，_volume:PWM占空比，100时蜂鸣器声音最大，代表PWM占空比为50%
 -  @retval None
   */
void buzzer_beep_set(uint16_t _tone_freq, uint8_t _volume)
{
    timer_parameter_struct timer_param_type;
    static uint16_t _period = 250 - 1;//默认是4000Hz
    static uint16_t _pulse = 250 - 1;//默认是4000Hz
    _period = (1000000/_tone_freq) -1;
    _pulse = ((_volume/100.0f)*_period)/2 -1;

    timer_param_type.alignedmode = TIMER_COUNTER_EDGE;
    timer_param_type.clockdivision = TIMER_CKDIV_DIV1;
    timer_param_type.counterdirection = TIMER_COUNTER_UP;
    timer_param_type.period = _period;   //4000Hz
    timer_param_type.prescaler = 168-1;   //120Mhz/120=1 000 000Hz
    timer_param_type.repetitioncounter = 0;
    timer_init(BEEP_TIMER, &timer_param_type);

    timer_channel_output_pulse_value_config(BEEP_TIMER, TIMER_CH_0, _pulse);
}


/**
 -  @brief  蜂鸣器引脚和定时器初始化
 -  @note   None
 -  @param  None
 -  @retval RT_EOK
   */
int beep_init(void)
{
    beep_pwm_gpio_init();
    beep_timer_init();
    return 1;
}
INIT_PREV_EXPORT(beep_init);
