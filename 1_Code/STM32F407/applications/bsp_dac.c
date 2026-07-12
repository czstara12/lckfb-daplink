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
#include <rtthread.h>
#include <drv_common.h>

/* Private variables ---------------------------------------------------------*/
DAC_HandleTypeDef hdac;
DMA_HandleTypeDef hdma_dac1;

TIM_HandleTypeDef htim2;

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
  * @brief DAC Initialization Function
  * @param None
  * @retval None
 */
static void MX_DAC_Init(void)
{

  /* USER CODE BEGIN DAC_Init 0 */

  /* USER CODE END DAC_Init 0 */

  DAC_ChannelConfTypeDef sConfig = {0};

  /* USER CODE BEGIN DAC_Init 1 */

  /* USER CODE END DAC_Init 1 */

  /** DAC Initialization
   */
  hdac.Instance = DAC;
  if (HAL_DAC_Init(&hdac) != HAL_OK)
  {
    Error_Handler();
  }

  /** DAC channel OUT1 config
   */
  sConfig.DAC_Trigger = DAC_TRIGGER_T2_TRGO;
  sConfig.DAC_OutputBuffer = DAC_OUTPUTBUFFER_ENABLE;
  if (HAL_DAC_ConfigChannel(&hdac, &sConfig, DAC_CHANNEL_1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN DAC_Init 2 */

  /* USER CODE END DAC_Init 2 */

}

/**
  * @brief TIM2 Initialization Function
  * @param None
  * @retval None
 */
static void MX_TIM2_Init(void)
{

  /* USER CODE BEGIN TIM2_Init 0 */

  /* USER CODE END TIM2_Init 0 */

  TIM_ClockConfigTypeDef sClockSourceConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};

  /* USER CODE BEGIN TIM2_Init 1 */

  /* USER CODE END TIM2_Init 1 */
  htim2.Instance = TIM2;
  htim2.Init.Prescaler = 0;
  htim2.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim2.Init.Period = 84-1;
  htim2.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim2.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;
  if (HAL_TIM_Base_Init(&htim2) != HAL_OK)
  {
    Error_Handler();
  }
  sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
  if (HAL_TIM_ConfigClockSource(&htim2, &sClockSourceConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_UPDATE;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim2, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM2_Init 2 */

  /* USER CODE END TIM2_Init 2 */

}

/**
 -  @brief  DAC1-PA4引脚初始化
 -  @note   None
 -  @param  None
 -  @retval None
   */
static void dac_gpio_init(void)
{
    // servo dac gpio init
    MX_DAC_Init();
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
  /* DMA1_Stream5_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA1_Stream5_IRQn, 2, 0);
  HAL_NVIC_EnableIRQ(DMA1_Stream5_IRQn);

  dac_gpio_init();

}

void switch_waveform(WaveformType new_wave)
{
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
	HAL_TIM_Base_Start(&htim2);
	HAL_DAC_Stop_DMA(&hdac, DAC_CHANNEL_1);
    HAL_DAC_Start_DMA(&hdac, DAC_CHANNEL_1, (uint32_t *)waveform_array, transfer_count, DAC_ALIGN_12B_R);
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
  MX_TIM2_Init();
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


    if (period < DAC_TIMER_MIN_PERIOD)
    {
        period = DAC_TIMER_MIN_PERIOD;
    }

    if (period > DAC_TIMER_MAX_PERIOD)
    {
        period = DAC_TIMER_MAX_PERIOD;
    }
    TIM_MasterConfigTypeDef sMasterConfig = {0};

    /* USER CODE BEGIN TIM2_Init 1 */

    /* USER CODE END TIM2_Init 1 */
    htim2.Instance = TIM2;
    htim2.Init.Prescaler = psc - 1;
    htim2.Init.CounterMode = TIM_COUNTERMODE_UP;
    htim2.Init.Period = period-1;
    htim2.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
    htim2.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;
    if (HAL_TIM_Base_Init(&htim2) != HAL_OK)
    {
      Error_Handler();
    }
}

/**
  * @brief This function handles DMA1 stream5 global interrupt.
 */
void DMA1_Stream5_IRQHandler(void)
{
  /* USER CODE BEGIN DMA1_Stream5_IRQn 0 */

  /* USER CODE END DMA1_Stream5_IRQn 0 */
  HAL_DMA_IRQHandler(&hdma_dac1);
  /* USER CODE BEGIN DMA1_Stream5_IRQn 1 */

  /* USER CODE END DMA1_Stream5_IRQn 1 */
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
    dac_timer_init();
    dac_timer_frequency_set(1*1000);
    switch_waveform(SINE_WAVE);

    return 1;
}
INIT_PREV_EXPORT(dac_init);
