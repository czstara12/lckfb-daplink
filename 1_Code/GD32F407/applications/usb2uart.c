#include "board.h"
#include "dap_main.h"
#include "bitband.h"
#include "screens.h"
#include "lvgl_data_update.h"

// 定义UART2接收缓冲区，大小为2KB，32字节对齐
static __ALIGNED(32) uint8_t uart2_recv_buff[2 * 1024];
// 定义全局变量，用于记录UART发送数据的长度
static volatile uint32_t g_uart_tx_transfer_length = 0;

// 定义UART2的DMA接收相关的宏
#define UART2_RX_DMA_RCU            RCU_DMA0
#define UART2_RX_DMA                DMA0
#define UART2_RX_DMA_CH             DMA_CH1
#define UART2_RX_DMA_CH_IRQ         DMA0_Channel1_IRQn
#define UART2_RX_DMA_CH_IRQ_HANDLER DMA0_Channel1_IRQHandler


//串口2的宏定义
#define COM_UART2                         USART2
#define COM_UART2_IRQn                    USART2_IRQn
#define COM_UART2_CLK                     RCU_USART2
#define COM_UART2_TX_GPIO_CLK             RCU_GPIOD
#define COM_UART2_TX_GPIO_AF              GPIO_AF_7
#define COM_UART2_TX_PORT                 GPIOD
#define COM_UART2_TX_PIN                  GPIO_PIN_8
#define COM_UART2_RX_GPIO_CLK             RCU_GPIOD
#define COM_UART2_RX_GPIO_AF              GPIO_AF_7
#define COM_UART2_RX_PORT                 GPIOD
#define COM_UART2_RX_PIN                  GPIO_PIN_9
#define COM_UART2_BAUD                    115200U

static int uart2_init(void)
{
    /* enable GPIO clock */
    rcu_periph_clock_enable( COM_UART2_TX_GPIO_CLK);
    rcu_periph_clock_enable( COM_UART2_RX_GPIO_CLK);

    /* enable USART clock */
    rcu_periph_clock_enable(COM_UART2_CLK);

    /* connect port to USARTx_Tx */
    gpio_af_set(COM_UART2_TX_PORT, COM_UART2_TX_GPIO_AF, COM_UART2_TX_PIN);

    /* connect port to USARTx_Rx */
    gpio_af_set(COM_UART2_RX_PORT, COM_UART2_RX_GPIO_AF, COM_UART2_RX_PIN);

    /* configure USART Tx as alternate function push-pull */
    gpio_mode_set(COM_UART2_TX_PORT, GPIO_MODE_AF, GPIO_PUPD_PULLUP,COM_UART2_TX_PIN);
    gpio_output_options_set(COM_UART2_TX_PORT, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ,COM_UART2_TX_PIN);

    /* configure USART Rx as alternate function push-pull */
    gpio_mode_set(COM_UART2_RX_PORT, GPIO_MODE_AF, GPIO_PUPD_PULLUP,COM_UART2_RX_PIN);
    gpio_output_options_set(COM_UART2_RX_PORT, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ,COM_UART2_RX_PIN);

    /* USART configure */
    usart_deinit(COM_UART2);
    usart_baudrate_set(COM_UART2,COM_UART2_BAUD);
    usart_receive_config(COM_UART2, USART_RECEIVE_ENABLE);
    usart_transmit_config(COM_UART2, USART_TRANSMIT_ENABLE);
    usart_enable(COM_UART2);

    /* USART interrupt configuration */

    nvic_irq_enable(USART2_IRQn, 0, 0);

    return 1;
}

INIT_BOARD_EXPORT(uart2_init);

// UART2接收DMA配置函数
void uart2_rx_dma_config(void)
{
    // 定义DMA初始化结构体
    dma_single_data_parameter_struct dma_init_struct;

    // 使能UART2接收DMA时钟
    rcu_periph_clock_enable(UART2_RX_DMA_RCU);

    // 初始化指定的DMA通道
    dma_deinit(UART2_RX_DMA, UART2_RX_DMA_CH);

    // 设置DMA传输参数
    dma_init_struct.periph_addr = (uint32_t)&USART_DATA(USART2);
    dma_init_struct.periph_inc = DMA_PERIPH_INCREASE_DISABLE;
    dma_init_struct.memory0_addr = (uint32_t)uart2_recv_buff;
    dma_init_struct.memory_inc = DMA_MEMORY_INCREASE_ENABLE;
    dma_init_struct.periph_memory_width = DMA_PERIPH_WIDTH_8BIT;
    dma_init_struct.circular_mode = DMA_CIRCULAR_MODE_DISABLE;
    dma_init_struct.direction = DMA_PERIPH_TO_MEMORY;
    dma_init_struct.number = sizeof(uart2_recv_buff);
    dma_init_struct.priority = DMA_PRIORITY_ULTRA_HIGH;

    // 初始化DMA单次传输模式
    dma_single_data_mode_init(UART2_RX_DMA, UART2_RX_DMA_CH, &dma_init_struct);

    // 选择DMA通道的子外设
    dma_channel_subperipheral_select(UART2_RX_DMA, UART2_RX_DMA_CH, DMA_SUBPERI4);

    // 使能DMA通道
    dma_channel_enable(UART2_RX_DMA, UART2_RX_DMA_CH);

    // 使能DMA传输完成中断
    dma_interrupt_enable(UART2_RX_DMA, UART2_RX_DMA_CH, DMA_CHXCTL_FTFIE);

    // 使能DMA中断
    nvic_irq_enable(UART2_RX_DMA_CH_IRQ, 0, 0);

    // 配置USART使用DMA接收
    usart_dma_receive_config(USART2, USART_RECEIVE_DMA_ENABLE);

    // 禁用USART接收缓冲非空中断
    usart_interrupt_disable(USART2, USART_INT_RBNE);

    // 清除USART空闲标志
    usart_flag_clear(USART2, USART_FLAG_IDLE);
    // 使能USART空闲中断
    usart_interrupt_enable(USART2, USART_INT_IDLE);
	
	// 清除USART发送完成
    usart_flag_clear(USART2, USART_FLAG_TC);
    // 使能USART发送完成中断
    usart_interrupt_enable(USART2, USART_INT_TC);
}

// CDC UART配置回调函数
void chry_dap_usb2uart_uart_config_callback(struct cdc_line_coding *line_coding)
{
    // USART配置
    usart_deinit(USART2);
    usart_baudrate_set(USART2,line_coding->dwDTERate);
    if(line_coding->bParityType == 1)
    {
        usart_parity_config(USART2,USART_PM_ODD);
    }else if (line_coding->bParityType == 2)
    {
        usart_parity_config(USART2,USART_PM_EVEN);
    }else
    {
        usart_parity_config(USART2,USART_PM_NONE);
    }
    usart_word_length_set(USART2,line_coding->bDataBits);

    if(line_coding->bCharFormat == 1)
    {
        usart_stop_bit_set(USART2,USART_STB_1_5BIT);
    }else if (line_coding->bCharFormat == 2)
    {
        usart_stop_bit_set(USART2,USART_STB_2BIT);
    }else
    {
        usart_stop_bit_set(USART2,USART_STB_1BIT);
    }

    // 使能USART接收和发送
    usart_receive_config(USART2, USART_RECEIVE_ENABLE);
    usart_transmit_config(USART2, USART_TRANSMIT_ENABLE);
    usart_enable(USART2);

    // 配置UART接收DMA
    uart2_rx_dma_config();
}

// UART2接收DMA中断处理函数
void UART2_RX_DMA_CH_IRQ_HANDLER(void)
{
//    /* enter interrupt */
//    rt_interrupt_enter();
    // 检查DMA传输完成标志
    if (dma_interrupt_flag_get(UART2_RX_DMA, UART2_RX_DMA_CH, DMA_INT_FLAG_FTF) == SET)
    {
        // 清除DMA传输完成标志
        dma_interrupt_flag_clear(UART2_RX_DMA, UART2_RX_DMA_CH, DMA_INT_FLAG_FTF);
    }
//    /* leave interrupt */
//    rt_interrupt_leave();
}

// USART2中断处理函数
void USART2_IRQHandler(void)
{
    static uint32_t receive_len = 0;

//    /* enter interrupt */
//    rt_interrupt_enter();

    // 检查USART空闲中断标志
    if (usart_interrupt_flag_get(USART2, USART_INT_FLAG_IDLE) == SET)
    {
        // 读取USART数据寄存器以清除空闲中断标志
        usart_data_receive(USART2);

        // 禁用DMA通道
        dma_channel_disable(UART2_RX_DMA, UART2_RX_DMA_CH);

        // 获取DMA传输的数据长度
        receive_len = dma_transfer_number_get(UART2_RX_DMA, UART2_RX_DMA_CH);

        // 将接收到的数据写入环形缓冲区
        chry_ringbuffer_write(&g_uartrx, uart2_recv_buff, sizeof(uart2_recv_buff) - receive_len);

        if(current_screen_get() == SCREEN_UART_MONITOR)
        {
            if((sizeof(uart2_recv_buff) - receive_len) < CONFIG_UARTRX_RINGBUF_SIZE_FOR_LVGL/3)
            {
                // 将接收到的数据写入环形缓冲区 给LVGL中的串口监视器使用
                chry_ringbuffer_write(&g_uartrx_for_lvgl, uart2_recv_buff,
                                      sizeof(uart2_recv_buff) - receive_len);
            }
            else
            {
                //too much data,can not display
            }
        }

        // 重新配置UART接收DMA
        uart2_rx_dma_config();
    }
	// 检查USART发送完成中断标志
    if (usart_interrupt_flag_get(USART2, USART_INT_FLAG_TC) == SET)
    {
		//让RS485进入接收模式
		GPIOB_OUTPUT(8)=0;
		usart_flag_clear(USART2, USART_FLAG_TC);
	}
//    /* leave interrupt */
//    rt_interrupt_leave();
}

// 定义UART2的DMA发送相关的宏
#define UART2_TX_DMA_RCU            RCU_DMA0
#define UART2_TX_DMA                DMA0
#define UART2_TX_DMA_CH             DMA_CH3
#define UART2_TX_DMA_CH_IRQ         DMA0_Channel3_IRQn
#define UART2_TX_DMA_CH_IRQ_HANDLER DMA0_Channel3_IRQHandler

// CDC UART通过DMA发送数据函数
void chry_dap_usb2uart_uart_send_bydma(uint8_t *data, uint16_t len)
{
    // 定义DMA初始化结构体
    dma_single_data_parameter_struct dma_init_struct;
	
    // 记录要发送的数据长度
    g_uart_tx_transfer_length = len;

    // 使能UART2发送DMA时钟
    rcu_periph_clock_enable(UART2_TX_DMA_RCU);

    // 初始化指定的DMA通道
    dma_deinit(UART2_TX_DMA, UART2_TX_DMA_CH);

    // 设置DMA传输参数
    dma_init_struct.periph_addr = ((uint32_t)&USART_DATA(USART2));
    dma_init_struct.periph_inc = DMA_PERIPH_INCREASE_DISABLE;
    dma_init_struct.memory0_addr = (uint32_t)data;
    dma_init_struct.memory_inc = DMA_MEMORY_INCREASE_ENABLE;
    dma_init_struct.periph_memory_width = DMA_PERIPH_WIDTH_8BIT;
    dma_init_struct.circular_mode = DMA_CIRCULAR_MODE_DISABLE;
    dma_init_struct.direction = DMA_MEMORY_TO_PERIPH;
    dma_init_struct.number = len;
    dma_init_struct.priority = DMA_PRIORITY_MEDIUM;

    // 初始化DMA单次传输模式
    dma_single_data_mode_init(UART2_TX_DMA, UART2_TX_DMA_CH, &dma_init_struct);

    // 选择DMA通道的子外设
    dma_channel_subperipheral_select(UART2_TX_DMA, UART2_TX_DMA_CH, DMA_SUBPERI4);

    // 使能DMA通道
    dma_channel_enable(UART2_TX_DMA, UART2_TX_DMA_CH);

    // 使能DMA传输完成中断
    dma_interrupt_enable(UART2_TX_DMA, UART2_TX_DMA_CH, DMA_CHXCTL_FTFIE);

    // 使能DMA中断
    nvic_irq_enable(UART2_TX_DMA_CH_IRQ, 1, 1);

    // 配置USART使用DMA发送
    usart_dma_transmit_config(USART2, USART_TRANSMIT_DMA_ENABLE);
}

// UART2发送DMA中断处理函数
void UART2_TX_DMA_CH_IRQ_HANDLER(void)
{
//    /* enter interrupt */
//    rt_interrupt_enter();

    // 检查DMA传输完成标志
    if(dma_interrupt_flag_get(UART2_TX_DMA,UART2_TX_DMA_CH,DMA_INT_FLAG_FTF))
    {
        // 清除DMA传输完成标志
        dma_interrupt_flag_clear(UART2_TX_DMA,UART2_TX_DMA_CH,DMA_INT_FLAG_FTF);
        // 调用发送完成回调函数
        chry_dap_usb2uart_uart_send_complete(g_uart_tx_transfer_length);
    }

//    /* leave interrupt */
//    rt_interrupt_leave();
}
