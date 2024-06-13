/*
* 梁山派软硬件资料与相关扩展板软硬件资料官网全部开源
* 开发板官网：www.lckfb.com
* 技术支持常驻论坛，任何技术问题欢迎随时交流学习
* 立创论坛：club.szlcsc.com
* 关注bilibili账号：【立创开发板】，掌握我们的最新动态！
* 不靠卖板赚钱，以培养中国工程师为己任
* Change Logs:
* Date           Author       Notes
* 2024-02-21     LCKFB-yzh    first version
*/

#ifndef __ADC_CONVERT_H__
#define __ADC_CONVERT_H__

#include "board.h"

#define ADC_CH_CNT 7  //ADC通道数
#define ADC_VALUE_CNT 1  //每通道保存值个数
#define ADC_BUF_LEN (ADC_CH_CNT * ADC_VALUE_CNT)  //DMA缓冲区数据长度

// depends on your reference voltage
#define ADC_REF_VOLTAGE 3.3f
//#define ADC_REF_VOLTAGE 3.0f

typedef struct
{
    uint16_t v_5v0_measure;
    uint16_t i_5v0_measure;
    uint16_t v_3v3_measure;
    uint16_t i_3v3_measure;
    uint16_t v_pd_measure;
    uint16_t i_pd_measure1;
    uint16_t i_pd_measure2;
} adc_raw_data_t;

typedef struct
{
    float v_5v0_measure;
    float i_5v0_measure;
    float v_3v3_measure;
    float i_3v3_measure;
    float v_pd_measure;
    float i_pd_measure1;
    float i_pd_measure2;
} adc_filter_data_t;


#endif /* __ADC_CONVERT_H__ */
