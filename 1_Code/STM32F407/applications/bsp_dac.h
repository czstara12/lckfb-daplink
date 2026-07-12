/*
* 梁山派软硬件资料与相关扩展板软硬件资料官网全部开源
* 开发板官网：www.lckfb.com
* 技术支持常驻论坛，任何技术问题欢迎随时交流学习
* 立创论坛：club.szlcsc.com
* 关注bilibili账号：【立创开发板】，掌握我们的最新动态！
* 不靠卖板赚钱，以培养中国工程师为己任
* Change Logs:
* Date           Author       Notes
* 2024-05-15     LCKFB-yzh    first version
*/
#ifndef __BSP_DAC_H__
#define __BSP_DAC_H__

#include "stdint.h"
#include "adc_convert.h"

#define DAC_REF_VOLTAGE    ADC_REF_VOLTAGE

typedef enum {
    SINE_WAVE,
    SQUARE_WAVE,
    TRIANGLE_WAVE,
    TRAPEZOID_WAVE,
    RISING_SAWTOOTH_WAVE,
    FALLING_SAWTOOTH_WAVE,
	SELF_DEFINE_WAVE,
    SELF_VOLTAGE
} WaveformType;

/* defined the adc PWM timer -*/
#define DAC_TIMER           TIMER1
#define DAC_TIMER_CLK       RCU_TIMER1

#define DAC_TRIGGER_TRGO    DAC_TRIGGER_T1_TRGO

/* defined the DAC pin: PA4 -*/
#define DAC_GPIO_CLK      RCU_GPIOA
#define DAC_GPIO_PORT     GPIOA
#define DAC_GPIO_PUPD     GPIO_PUPD_NONE
#define DAC_GPIO_PIN      GPIO_PIN_4

void switch_waveform(WaveformType new_wave);
void dac_timer_frequency_set(uint32_t _frequency);
void dac_output_data_set(uint16_t data);

#endif //__BSP_DAC_H__
