#include "main.h"
#include "dap_main.h"
#include "drv_common.h"
#include "drv_gpio.h"
#include "screens.h"
#include "dap_main.h"
#include "lvgl_data_update.h"

#define RS485_RW_PIN GET_PIN(B, 8)

// 定义UART2接收缓冲区，大小为2KB，32字节对齐
static __ALIGNED(32) uint8_t uart3_recv_buff[2 * 1024];
static uint16_t uart3_rx_old_pos = 0;
// 定义全局变量，用于记录UART发送数据的长度
static volatile uint32_t g_uart_tx_transfer_length = 0;

UART_HandleTypeDef huart1;
UART_HandleTypeDef huart3;
DMA_HandleTypeDef hdma_usart3_rx;
DMA_HandleTypeDef hdma_usart3_tx;

/* USART1 init function */

int MX_USART1_UART_Init(void)
{

  /* USER CODE BEGIN USART1_Init 0 */

  /* USER CODE END USART1_Init 0 */

  /* USER CODE BEGIN USART1_Init 1 */

  /* USER CODE END USART1_Init 1 */
  huart1.Instance = USART1;
  huart1.Init.BaudRate = 115200;
  huart1.Init.WordLength = UART_WORDLENGTH_8B;
  huart1.Init.StopBits = UART_STOPBITS_1;
  huart1.Init.Parity = UART_PARITY_NONE;
  huart1.Init.Mode = UART_MODE_TX_RX;
  huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart1.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART1_Init 2 */
 return 1;
  /* USER CODE END USART1_Init 2 */

}

/* USART3 init function */

int MX_USART3_UART_Init(void)
{

  /* USER CODE BEGIN USART3_Init 0 */

  /* USER CODE END USART3_Init 0 */

  /* USER CODE BEGIN USART3_Init 1 */

  /* USER CODE END USART3_Init 1 */
  huart3.Instance = USART3;
  huart3.Init.BaudRate = 115200;
  huart3.Init.WordLength = UART_WORDLENGTH_8B;
  huart3.Init.StopBits = UART_STOPBITS_1;
  huart3.Init.Parity = UART_PARITY_NONE;
  huart3.Init.Mode = UART_MODE_TX_RX;
  huart3.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart3.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart3) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART3_Init 2 */
 return 1;
  /* USER CODE END USART3_Init 2 */

}

/**
  * Enable DMA controller clock
  */
int MX_DMA_Init(void)
{

  /* DMA controller clock enable */
  __HAL_RCC_DMA1_CLK_ENABLE();

  /* DMA interrupt init */
  /* DMA1_Stream1_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA1_Stream1_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(DMA1_Stream1_IRQn);
  /* DMA1_Stream3_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA1_Stream3_IRQn, 1, 0);
  HAL_NVIC_EnableIRQ(DMA1_Stream3_IRQn);

  return 1;
}


int usb2uart_init(void)
{
	rt_pin_mode(RS485_RW_PIN, PIN_MODE_OUTPUT);
	rt_pin_write(RS485_RW_PIN, PIN_LOW);
	MX_DMA_Init();
	MX_USART3_UART_Init();
	MX_USART1_UART_Init();
	return 1;
}
INIT_BOARD_EXPORT(usb2uart_init);

// CDC UART配置回调函数
void chry_dap_usb2uart_uart_config_callback(struct cdc_line_coding *line_coding)
{
    // USART配置
	if (HAL_UART_AbortReceive(&huart3) != HAL_OK)
	{
		Error_Handler();
	}
	
	huart3.Instance = USART3;
	huart3.Init.BaudRate = line_coding->dwDTERate;
    if(line_coding->bParityType == 1)
    {
		huart3.Init.Parity = UART_PARITY_ODD;
    }else if (line_coding->bParityType == 2)
    {
		huart3.Init.Parity = UART_PARITY_EVEN;
    }else
    {
		huart3.Init.Parity = UART_PARITY_NONE;
    }
	huart3.Init.WordLength = (huart3.Init.Parity != UART_PARITY_NONE &&
							 line_coding->bDataBits == 8) ? UART_WORDLENGTH_9B : UART_WORDLENGTH_8B;
   
	if(line_coding->bCharFormat == 1)
	{
			huart3.Init.StopBits = UART_STOPBITS_1_5;
    }else if (line_coding->bCharFormat == 2)
    {
		huart3.Init.StopBits = UART_STOPBITS_2;
    }else
    {
		huart3.Init.StopBits = UART_STOPBITS_1;
    }
	
	huart3.Init.HwFlowCtl = UART_HWCONTROL_NONE;
	huart3.Init.OverSampling = UART_OVERSAMPLING_16;
	huart3.Init.Mode = UART_MODE_TX_RX;
	if (HAL_UART_Init(&huart3) != HAL_OK)
    {
    Error_Handler();
    }

    // 配置UART接收DMA
	uart3_rx_old_pos = 0;
	if (HAL_UARTEx_ReceiveToIdle_DMA(&huart3, uart3_recv_buff, sizeof(uart3_recv_buff)) != HAL_OK)
	{
		Error_Handler();
	}
}

static void uart3_receive_write(uint8_t *data, uint16_t length)
{
	chry_ringbuffer_write(&g_uartrx, data, length);
	if (current_screen_get() == SCREEN_UART_MONITOR &&
		length < CONFIG_UARTRX_RINGBUF_SIZE_FOR_LVGL / 3)
	{
		chry_ringbuffer_write(&g_uartrx_for_lvgl, data, length);
	}
}

/**
 * @brief 处理 USART3 循环 DMA 新接收的数据区段。
 *
 * @param huart UART 句柄。
 * @param position DMA 缓冲区当前写入位置。
 */
void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t position)
{
	if (huart->Instance != USART3 || position == uart3_rx_old_pos)
	{
		return;
	}

	if (position > uart3_rx_old_pos)
	{
		uart3_receive_write(&uart3_recv_buff[uart3_rx_old_pos], position - uart3_rx_old_pos);
	}
	else
	{
		uart3_receive_write(&uart3_recv_buff[uart3_rx_old_pos], sizeof(uart3_recv_buff) - uart3_rx_old_pos);
		uart3_receive_write(uart3_recv_buff, position);
	}

	uart3_rx_old_pos = position == sizeof(uart3_recv_buff) ? 0 : position;
}

// USART3中断处理函数
void USART3_IRQHandler(void)
{
	HAL_UART_IRQHandler(&huart3);
	__HAL_UART_CLEAR_OREFLAG(&huart3);
}


// CDC UART通过DMA发送数据函数
void chry_dap_usb2uart_uart_send_bydma(uint8_t *data, uint16_t len)
{

	rt_pin_write(RS485_RW_PIN, PIN_HIGH);
	HAL_UART_Transmit_DMA(&huart3,data,len);
	
	
	// 记录要发送的数据长度
    g_uart_tx_transfer_length = len;

}
void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart)
{
	if(huart->Instance == USART3)
	{
		rt_pin_write(RS485_RW_PIN, PIN_LOW);
		chry_dap_usb2uart_uart_send_complete(g_uart_tx_transfer_length);
	}
}

// UART3发送DMA中断处理函数
void DMA1_Stream3_IRQHandler(void)
{
	HAL_DMA_IRQHandler(&hdma_usart3_tx);
}

// UART3接收DMA中断处理函数
void DMA1_Stream1_IRQHandler(void)
{
	HAL_DMA_IRQHandler(&hdma_usart3_rx);
}

//// UART3发送DMA中断处理函数
//void DMA1_Stream3_IRQHandler(void)
//{
//    // 检查DMA传输完成标志
//    if(__HAL_DMA_GET_FLAG(&hdma_usart3_tx,DMA_FLAG_TCIF3_7)!= RESET)
//    {
//        // 清除DMA传输完成标志
//        __HAL_DMA_CLEAR_FLAG(&hdma_usart3_tx,DMA_FLAG_TCIF3_7);
//        // 调用发送完成回调函数
//        chry_dap_usb2uart_uart_send_complete(g_uart_tx_transfer_length);
//    }
//}
