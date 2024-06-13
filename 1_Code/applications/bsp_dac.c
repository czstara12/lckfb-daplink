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

//参考资料：https://blog.51cto.com/bruceou/5531597

#include <stdio.h>
#include <board.h>
#include <bsp_dac.h>

// 正弦波
__attribute__((aligned(4))) static uint16_t Sine12bit[32] = {
    2448, 2832, 3186, 3496, 3751, 3940, 4057, 4095, 4057, 3940, 3751,
    3496, 3186, 2832, 2448, 2048, 1648, 1264, 910,  600,  345,  156,
    39,   0,    39,   156,  345,  600,  910,  1264, 1648, 2048};
// 方波
__attribute__((aligned(4))) static uint16_t Square12bit[32] = {
    4095, 4095, 4095, 4095, 4095, 4095, 4095, 4095, 4095, 4095, 4095,
    4095, 4095, 4095, 4095, 4095, 0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0};
// 三角波
__attribute__((aligned(4))) static uint16_t Triangle12bit[32] = {
    0,    256,  512,  768,  1024, 1280, 1536, 1792, 2048, 2304, 2560,
    2816, 3072, 3328, 3584, 3840, 4095, 3840, 3584, 3328, 3072, 2816,
    2560, 2304, 2048, 1792, 1536, 1280, 1024, 768,  512,  256};
// 梯形波
__attribute__((aligned(4))) static uint16_t Trapezoid12bit[32] = {
    0,    512,  1024, 1536, 2048, 2560, 3072, 3584, 4095, 4095, 4095,
    4095, 4095, 4095, 4095, 4095, 4095, 3584, 3072, 2560, 2048, 1536,
    1024, 512,  0,    0,    0,    0,    0,    0,    0,    0};
// 上升斜坡锯齿波
__attribute__((aligned(4))) static uint16_t RisingSawtooth12bit[32] = {
    0,    128,  256,  384,  512,  640,  768,  896,  1024, 1152, 1280,
    1408, 1536, 1664, 1792, 1920, 2048, 2176, 2304, 2432, 2560, 2688,
    2816, 2944, 3072, 3200, 3328, 3456, 3584, 3712, 3840, 3968};
// 下降斜坡锯齿波
__attribute__((aligned(4))) static uint16_t FallingSawtooth12bit[32] = {
    4095, 3968, 3840, 3712, 3584, 3456, 3328, 3200, 3072, 2944, 2816,
    2688, 2560, 2432, 2304, 2176, 2048, 1920, 1792, 1664, 1536, 1408,
    1280, 1152, 1024, 896,  768,  640,  512,  384,  256,  128};
// 自定义波形
__attribute__((aligned(4))) uint16_t SelfWave12bit[32] = {
    0, 0, 0, 0, 0, 0, 0, 0,  0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
};
//输出12bit电压值
__attribute__((aligned(4))) static uint16_t SelfVoltage12bit[1] = {
    0
};

/**
 -  @brief  DAC1-PA5引脚初始化
 -  @note   None
 -  @param  None
 -  @retval None
   */
static void dac_gpio_init(void)
{
    // servo dac gpio init
    rcu_periph_clock_enable(DAC_GPIO_CLK);

    gpio_mode_set(DAC_GPIO_PORT, GPIO_MODE_ANALOG, DAC_GPIO_PUPD, DAC_GPIO_PIN);

    return;
}

/*
    brief      Configure the DAC peripheral
    param[in]  none
    param[out] none
    retval     none
*/
static void dac_config(void)
{
    adc_clock_config(ADC_ADCCK_HCLK_DIV5);    //33.6MHz
    /* DAC GPIO configuration */
    dac_gpio_init();

    dac_trigger_disable(DAC1);

    /* enable the clock of DAC */
    rcu_periph_clock_enable(RCU_DAC);

    dac_wave_mode_config(DAC1, DAC_WAVE_DISABLE);
	
    dac_output_buffer_enable(DAC1);
	
    /* configure the DAC1 */
    dac_trigger_source_config(DAC1, DAC_TRIGGER_TRGO);

    /* DAC的DMA功能使能 */
    dac_dma_enable(DAC1);

    dac_trigger_enable(DAC1);

	dac_concurrent_disable();
    /* enable DAC0 and set data */
    dac_enable(DAC1);

    dac_data_set(ADC1, DAC_ALIGN_12B_R, 0x00);
}

void switch_waveform(WaveformType new_wave)
{
	dma_single_data_parameter_struct dma_init_struct;
    static uint16_t *waveform_array;

    static uint16_t transfer_count = 0;
    switch (new_wave)
    {
    case SINE_WAVE:
        waveform_array = Sine12bit;
        transfer_count = 32;
        break;
    case SQUARE_WAVE:
        waveform_array = Square12bit;
        transfer_count = 32;
        break;
    case TRIANGLE_WAVE:
        waveform_array = Triangle12bit;
        transfer_count = 32;
        break;
    case TRAPEZOID_WAVE:
        waveform_array = Trapezoid12bit;
        transfer_count = 32;
        break;
    case RISING_SAWTOOTH_WAVE:
        waveform_array = RisingSawtooth12bit;
        transfer_count = 32;
        break;
    case FALLING_SAWTOOTH_WAVE:
        waveform_array = FallingSawtooth12bit;
        transfer_count = 32;
        break;
    case SELF_DEFINE_WAVE:
        waveform_array = SelfWave12bit;
        transfer_count = 32;
        break;
    case SELF_VOLTAGE:
        waveform_array = SelfVoltage12bit;
        transfer_count = 1;
        break;
    default:
        waveform_array = Sine12bit; // 默认返回正弦波
        transfer_count = 32;
        break;
    }

    /* enable DMA CLK */
    rcu_periph_clock_enable(RCU_DMA0);

    /* deinitialize DMA0 channel6 */
    dma_deinit(DMA0, DMA_CH6);

	dma_single_data_para_struct_init(&dma_init_struct);
	
    dma_init_struct.direction = DMA_MEMORY_TO_PERIPH;/* 存储器到外设方向 */
    dma_init_struct.memory0_addr = (uint32_t)waveform_array;    /* 存储器基地址 */
    dma_init_struct.memory_inc = DMA_MEMORY_INCREASE_ENABLE;
    dma_init_struct.periph_memory_width = DMA_PERIPH_WIDTH_16BIT;
    dma_init_struct.periph_inc = DMA_PERIPH_INCREASE_DISABLE;
    dma_init_struct.periph_addr = ((uint32_t)(&DAC1_R12DH));
	dma_init_struct.circular_mode = DMA_CIRCULAR_MODE_ENABLE;
    dma_init_struct.number = transfer_count;            /* 传输数据个数 */
    dma_init_struct.priority = DMA_PRIORITY_ULTRA_HIGH;

    dma_single_data_mode_init(DMA0, DMA_CH6, &dma_init_struct);
    dma_channel_subperipheral_select(DMA0, DMA_CH6, DMA_SUBPERI7);


    //DMA循环模式开启
    dma_circulation_enable(DMA0, DMA_CH6);

    dma_channel_enable(DMA0, DMA_CH6);
}

void dac_output_data_set(uint16_t data)
{
    switch_waveform(SELF_VOLTAGE);
    if(data >= 4095)
    {
        data = 4095;
    }
    SelfVoltage12bit[0] = data;
}

/**
 -  @brief  DAC定时器初始化
 -  @note   None
 -  @param  None
 -  @retval None
   */
static void dac_timer_init(void)
{
    timer_parameter_struct timer_initpara;

    //Enable TIMER clock
    rcu_periph_clock_enable(DAC_TIMER_CLK);

    timer_deinit(DAC_TIMER);

	//84Mhz / 10 / 32 = 0.2625Mhz
    /* TIMER configuration */
    timer_initpara.prescaler         = 0;
    timer_initpara.alignedmode       = TIMER_COUNTER_EDGE;
    timer_initpara.counterdirection  = TIMER_COUNTER_UP;
    timer_initpara.period            = 10-1;
    timer_initpara.clockdivision     = TIMER_CKDIV_DIV1;
    timer_init(DAC_TIMER, &timer_initpara);

    //定时器主输出触发源选择
    timer_master_output_trigger_source_select(DAC_TIMER,TIMER_TRI_OUT_SRC_UPDATE);

    //定时器更新事件使能
    timer_update_event_enable(DAC_TIMER);
    /* TIMER enable */
    timer_enable(DAC_TIMER);

}


#define DAC_TIMER_MAX_PERIOD 65535
#define DAC_TIMER_MIN_PERIOD (5-1)


void dac_timer_frequency_set(uint32_t _frequency)
{
    rt_uint32_t period, _period;
    rt_uint64_t tim_clock, psc;

	_frequency *= 32;  //32个数组数据

    _period = (uint32_t)(( (double )1.0f / (double )_frequency) *  (double )1000000000.0f);

    tim_clock = 84*1000*1000;

    /* Convert nanosecond to frequency and duty cycle. 1s = 1 * 1000 * 1000 * 1000 ns */
    tim_clock /= 1000000UL;
    period = _period * tim_clock / 1000ULL;
    psc = period / DAC_TIMER_MAX_PERIOD + 1;
    period = period / psc;

    timer_prescaler_config(DAC_TIMER, psc - 1, TIMER_PSC_RELOAD_NOW);

    if (period < DAC_TIMER_MIN_PERIOD)
    {
        period = DAC_TIMER_MIN_PERIOD;
    }

    timer_autoreload_value_config(DAC_TIMER, period - 1);
}


/**
 -  @brief  DAC引脚和定时器初始化
 -  @note   None
 -  @param  None
 -  @retval RT_EOK
   */
int dac_init(void)
{
    dac_config();
	switch_waveform(SINE_WAVE);
    dac_timer_init();
    dac_timer_frequency_set(1*1000);

    return 1;
}
INIT_PREV_EXPORT(dac_init);
