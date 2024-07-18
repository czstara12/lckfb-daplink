/*
 * 立创开发板软硬件资料与相关扩展板软硬件资料官网全部开源
 * 开发板官网：www.lckfb.com
 * 技术支持常驻论坛，任何技术问题欢迎随时交流学习
 * 立创论坛：club.szlcsc.com
 * 关注bilibili账号：【立创开发板】，掌握我们的最新动态！
 * 不靠卖板赚钱，以培养中国工程师为己任
 * Change Logs:
 * Date           Author       Notes
 * 2024-02-21     LCKFB-yzh    first version
 */
#include "board.h"
#include "adc_convert.h"
#include <uMCN.h>
#include "Filter.h"

#define LOG_TAG     "adc_convert"     // 该模块对应的标签。不定义时，默认：NO_TAG
#define LOG_LVL     LOG_LVL_DBG   // 该模块对应的日志输出级别。不定义时，默认：调试级别
#include <ulog.h>                 // 必须在 LOG_TAG 与 LOG_LVL 下面

#define PD_I_LEVEL2_ENABLE GET_PIN(B, 11)

static adc_raw_data_t adc_raw_data;
static adc_filter_data_t adc_filter_data;

static McnNode_t adc_raw_data_node;

/* 定时器的控制块 */
static rt_timer_t adc_filter_data_timer;

MCN_DEFINE(adc_raw_data_topic, sizeof(adc_raw_data_t));
MCN_DEFINE(adc_filter_data_topic, sizeof(adc_filter_data_t));

rt_inline void open_pd_measure_level2(void)
{
    GPIOB_OUTPUT(11) = 0;
}

rt_inline void close_pd_measure_level2(void)
{
    GPIOB_OUTPUT(11) = 1;
}

/**
 * ADC采集引脚信息：
 * V_5V0_OUT_MEASURE: PC00 ADC012_IN10
 * I_5V0_OUT_MEASURE: PC01 ADC012_IN11
 * V_3V3_OUT_MEASURE: PC02 ADC012_IN12
 * I_3V3_OUT_MEASURE: PC03 ADC012_IN13
 * V_PD_OUT_MEASURE:  PC04 ADC012_IN14
 * I_PD_OUT_MEASURE1:  PC05 ADC012_IN15
 * I_PD_OUT_MEASURE2:  PA03 ADC012_IN3
 */

/**
 * 初始化ADC采集的引脚。
 */
static void adc_gpio_config(void)
{
    rcu_periph_clock_enable(RCU_GPIOC); // 使能GPIOC时钟
    gpio_mode_set(GPIOC, GPIO_MODE_ANALOG, GPIO_PUPD_NONE,
                  GPIO_PIN_0); // 将PC0设置为模拟输入模式
    gpio_mode_set(GPIOC, GPIO_MODE_ANALOG, GPIO_PUPD_NONE,
                  GPIO_PIN_1); // 将PC1设置为模拟输入模式
    gpio_mode_set(GPIOC, GPIO_MODE_ANALOG, GPIO_PUPD_NONE,
                  GPIO_PIN_2); // 将PC2设置为模拟输入模式
    gpio_mode_set(GPIOC, GPIO_MODE_ANALOG, GPIO_PUPD_NONE,
                  GPIO_PIN_3); // 将PC3设置为模拟输入模式
    gpio_mode_set(GPIOC, GPIO_MODE_ANALOG, GPIO_PUPD_NONE,
                  GPIO_PIN_4); // 将PC4设置为模拟输入模式
    gpio_mode_set(GPIOC, GPIO_MODE_ANALOG, GPIO_PUPD_NONE,
                  GPIO_PIN_5); // 将PC5设置为模拟输入模式

    rcu_periph_clock_enable(RCU_GPIOA); // 使能GPIOA时钟
    gpio_mode_set(GPIOA, GPIO_MODE_ANALOG, GPIO_PUPD_NONE,
                  GPIO_PIN_3); // 将PA3设置为模拟输入模式

    rt_pin_mode(PD_I_LEVEL2_ENABLE, PIN_MODE_OUTPUT);
    rt_pin_write(PD_I_LEVEL2_ENABLE, PIN_HIGH);//默认高电平就是开启对应档位的MOS管（PD_I_LEVEL2_ENABLE）
	
	// GPIOB_OUTPUT(11) = 0; // 开启第二个档位，关闭第二个档位的MOS管
    close_pd_measure_level2();
}



//参考来源：https://blog.csdn.net/madao1234/article/details/123876688

/**
 * 初始化ADC
 */
static void adc_config(void)
{
    // ADC 独立模式
    adc_sync_mode_config(ADC_SYNC_MODE_INDEPENDENT);
    // 连续转换模式使能
    adc_special_function_config(ADC0, ADC_CONTINUOUS_MODE, ENABLE);
    // Scan mode enable
    adc_special_function_config(ADC0, ADC_SCAN_MODE, ENABLE);
    // ADC data右对齐
    adc_data_alignment_config(ADC0, ADC_DATAALIGN_RIGHT);

    // 规则通道数量
    adc_channel_length_config(ADC0, ADC_ROUTINE_CHANNEL, ADC_CH_CNT);
    // ADC规则通道配置：Rank优先级排序（0~15），采样时间：144个周期
    adc_routine_channel_config(ADC0, 0, ADC_CHANNEL_10, ADC_SAMPLETIME_480);
    adc_routine_channel_config(ADC0, 1, ADC_CHANNEL_11, ADC_SAMPLETIME_480);
    adc_routine_channel_config(ADC0, 2, ADC_CHANNEL_12, ADC_SAMPLETIME_480);
    adc_routine_channel_config(ADC0, 3, ADC_CHANNEL_13, ADC_SAMPLETIME_480);
    adc_routine_channel_config(ADC0, 4, ADC_CHANNEL_14, ADC_SAMPLETIME_480);
    adc_routine_channel_config(ADC0, 5, ADC_CHANNEL_15, ADC_SAMPLETIME_480);
    adc_routine_channel_config(ADC0, 6, ADC_CHANNEL_3, ADC_SAMPLETIME_480);


    adc_resolution_config(
        ADC0, ADC_RESOLUTION_12B); // 12位分辨率
                                   // ADC外部触发禁用, 即只能使用软件触发
    adc_external_trigger_config(ADC0, ADC_ROUTINE_CHANNEL,
                                EXTERNAL_TRIGGER_DISABLE);

    // ADC DMA function enable
    adc_dma_request_after_last_enable(
        ADC0); // 每个规则组通道转换完成后发送一个DMA请求
    adc_dma_mode_enable(ADC0);

    adc_enable(ADC0);
    // wait for ADC stability
//    rt_thread_mdelay(1);
    // ADC自校准
    adc_calibration_enable(ADC0);
}

static volatile uint16_t gt_adc_val[ADC_BUF_LEN];  //DMA缓冲区

static void dma_config(void)
{
    // ADC0 DMA
    dma_single_data_parameter_struct dma_single_data_parameter;

    // ADC DMA_channel configuration
    dma_deinit(DMA1, DMA_CH0);

    // initialize DMA single data mode
    dma_single_data_parameter.periph_addr = (uint32_t)(&ADC_RDATA(ADC0));   // peripheral address
    dma_single_data_parameter.periph_inc = DMA_PERIPH_INCREASE_DISABLE;
    dma_single_data_parameter.memory0_addr = (uint32_t)(gt_adc_val);        // memory address
    dma_single_data_parameter.memory_inc = DMA_MEMORY_INCREASE_ENABLE;
    dma_single_data_parameter.periph_memory_width = DMA_PERIPH_WIDTH_16BIT;
    dma_single_data_parameter.direction = DMA_PERIPH_TO_MEMORY;             // transfer direction
    dma_single_data_parameter.number = ADC_CH_CNT;                          // transfer length

    dma_single_data_parameter.priority = DMA_PRIORITY_ULTRA_HIGH;
    dma_single_data_mode_init(DMA1, DMA_CH0, &dma_single_data_parameter);
    dma_channel_subperipheral_select(DMA1, DMA_CH0, DMA_SUBPERI0);

    nvic_irq_enable(DMA1_Channel0_IRQn, 2, 2);                   //使能 DMA0 通道 1 的 NVIC
    dma_interrupt_enable(DMA1, DMA_CH0, DMA_CHXCTL_FTFIE);            //开启接收完成中断


    // 使能循环模式
    dma_circulation_enable(DMA1, DMA_CH0);
}

static uint64_t s_iADCFlag = {0};

//static float _v_5v0 = 0.0f;
//static float _v_PD = 0.0f;

//static float _i_5v0 = 0.0f;

static float _i_PD = 0.0f;
//static float _i_PD3 = 0.0f;
static float _i_PD2 = 0.0f;
static float _i_PD1 = 0.0f;


//static float _filte_v_5V0 = 0.0f;
//static float _filte_i_5V0 = 0.0f;

// 中断频率高了之后频繁调用RT-Thread的rt_interrupt_enter
// 可能会造成串口接收数据被阶段，导致数据丢失。 参考文章
// https://club.rt-thread.org/ask/question/a27fbb1a84f3dad0.html
// 在Cortex_M系列MCU中，当没使用信号情况下，中断里进行其它“事件、消息、信号量”等内核操作，不进行中断通知是没有影响的，但在其它Cortex系列MCU中，情况就不一样了。


void DMA1_Channel0_IRQHandler(void)
{
    //TODO:考虑先存数据，后面做了滤波处理后再算。

    // _v_5v0 = (gt_adc_val[0]/4095.0f)*ADC_REF_VOLTAGE*2.0f;     //V
    // _i_5v0 =  (gt_adc_val[1]/4095.0f)*ADC_REF_VOLTAGE/(0.12f*50)*1000;  //ma
    //
    // _v_PD = (gt_adc_val[4]/4095.0f)*ADC_REF_VOLTAGE*7.2463f;     //V

    _i_PD1 =  (gt_adc_val[5]/4095.0f)*ADC_REF_VOLTAGE/(0.012f*50)*1000;  //mA
    _i_PD2 =  (gt_adc_val[6]/4095.0f)*ADC_REF_VOLTAGE/(0.3f*50)*1000;  //mA

	
	_i_PD = _i_PD1;
   if (_i_PD < 150)
   {
       open_pd_measure_level2();
       _i_PD = _i_PD2;
   }
   if (_i_PD2 >= 200)
   {
       close_pd_measure_level2();
   }

    if(dma_interrupt_flag_get(DMA1, DMA_CH0, DMA_INT_FLAG_FTF))
    {
        // 清除中断标志位
        dma_interrupt_flag_clear(DMA1, DMA_CH0, DMA_INT_FLAG_FTF);

        // 标记已经接收到一批数据
        s_iADCFlag++;

        adc_raw_data.v_5v0_measure=gt_adc_val[0];
        adc_raw_data.i_5v0_measure=gt_adc_val[1];
        adc_raw_data.v_3v3_measure=gt_adc_val[2];
        adc_raw_data.i_3v3_measure=gt_adc_val[3];
        adc_raw_data.v_pd_measure=gt_adc_val[4];
        adc_raw_data.i_pd_measure1=gt_adc_val[5];
        adc_raw_data.i_pd_measure2=gt_adc_val[6];

        mcn_publish(MCN_HUB(adc_raw_data_topic), &adc_raw_data);


    }

}

/* adc filter 定时器 1 超时函数 */
static void adc_filter_data_handler(void *parameter)
{

    adc_raw_data_t _adc_raw_data;

    if (mcn_poll(adc_raw_data_node))
    {
        mcn_copy(MCN_HUB(adc_raw_data_topic), adc_raw_data_node,
                 &_adc_raw_data);
        adc_filter_data.v_5v0_measure =
            (Filter_SlidingWindowAvg(0, adc_raw_data.v_5v0_measure) / 4095.0f)
            * 3.3f * 2.0f; // V
        adc_filter_data.i_5v0_measure =
            (Filter_SlidingWindowAvg(1, adc_raw_data.i_5v0_measure) / 4095.0f)
            * 3.3f / (0.12f * 50) * 1000; // ma

        adc_filter_data.v_3v3_measure =
            (Filter_SlidingWindowAvg(2, adc_raw_data.v_3v3_measure) / 4095.0f)
            * 3.3f * 1.2f; // V
        adc_filter_data.i_3v3_measure =
            (Filter_SlidingWindowAvg(3, adc_raw_data.i_3v3_measure) / 4095.0f)
            * 3.3f / (0.12f * 50) * 1000; // ma

        adc_filter_data.v_pd_measure =
            (Filter_SlidingWindowAvg(4, adc_raw_data.v_pd_measure) / 4095.0f)
            * 3.3f * 7.2463f; // V

        adc_filter_data.i_pd_measure1 =
            (Filter_SlidingWindowAvg(5, adc_raw_data.i_pd_measure1) / 4095.0f)
            * 3.3f / (0.012f * 50) * 1000; // ma
        adc_filter_data.i_pd_measure2 =
            (Filter_SlidingWindowAvg(6, adc_raw_data.i_pd_measure2) / 4095.0f)
            * 3.3f / (0.3f * 50) * 1000; // ma

        mcn_publish(MCN_HUB(adc_filter_data_topic), &adc_filter_data);
    }
}

// 启动ADC转换
void adc_convert_start(void)
{
    dma_channel_enable(DMA1, DMA_CH0); // 启动DMA
    adc_software_trigger_enable(ADC0, ADC_ROUTINE_CHANNEL); // 开启软件触发ADC转换
}

static int adc_raw_data_topic_echo(void *param)
{
    adc_raw_data_t data;
    if (mcn_copy_from_hub((McnHub *)param, &data) == RT_EOK)
    {
        rt_kprintf("adc raw data:%4d,%4d,%4d,%4d,%4d,%4d,%4d\n", data.v_5v0_measure,
                   data.i_5v0_measure, data.v_3v3_measure, data.i_3v3_measure,
                   data.v_pd_measure, data.i_pd_measure1, data.i_pd_measure2);
        return 0;
    }
    return -1;
}

static int adc_filter_data_topic_echo(void *param)
{
    adc_filter_data_t data;
    if (mcn_copy_from_hub((McnHub *)param, &data) == RT_EOK)
    {
        LOG_I(
            "adc filter data:%2.5f,%2.5f,%2.5f,%2.5f,%2.5f,%2.5f,%2.5f\n",
            data.v_5v0_measure, data.i_5v0_measure, data.v_3v3_measure,
            data.i_3v3_measure, data.v_pd_measure, data.i_pd_measure1,
            data.i_pd_measure2);
        return 0;
    }
    return -1;
}

/*!
    \brief      adc_convert_init function
    \param[in]  none
    \param[out] none
    \retval     none
*/
int adc_convert_init(void)
{
    adc_gpio_config();

    /* enable ADC clock */
    rcu_periph_clock_enable(RCU_ADC0);
    /* config ADC clock */
    adc_clock_config(ADC_ADCCK_HCLK_DIV5);

    /* enable DMA1 clock */
    rcu_periph_clock_enable(RCU_DMA1);

    adc_config();

    dma_config();

    adc_convert_start();

    mcn_advertise(MCN_HUB(adc_raw_data_topic), adc_raw_data_topic_echo);
    mcn_advertise(MCN_HUB(adc_filter_data_topic), adc_filter_data_topic_echo);

    adc_raw_data_node =
        mcn_subscribe(MCN_HUB(adc_raw_data_topic), RT_NULL, RT_NULL);

    /* 创建adc滤波定时器 周期定时器 */
    adc_filter_data_timer =
        rt_timer_create("adc_filter_timer", adc_filter_data_handler, RT_NULL, 1,
                        RT_TIMER_FLAG_PERIODIC);

    if (adc_filter_data_timer != RT_NULL)
        rt_timer_start(adc_filter_data_timer);
    return 1;
}

INIT_PREV_EXPORT(adc_convert_init);
