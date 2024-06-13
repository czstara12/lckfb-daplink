/*
 * Copyright (c) 2006-2024, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2024-02-18     yuanzihao    first implementation
 */

#include <stdio.h>
#include <rtthread.h>
#include <rtdevice.h>
#include <board.h>
#include <bsp_beep.h>
#include <adc_convert.h>
#include <power_control.h>

#include "bsp_led.h"

#include "dap_main.h"

#define RUNNING_LED_PIN GET_PIN(B, 2)

#define RS485_RW_PIN GET_PIN(B, 8)

#if 0
static void send_to_vofa_callback(void* parameter);

static char turned_buf[200];
static float v_5v0 = 0.0f;
static float i_5v0 = 0.0f;
static uint16_t turned_size = 0;



static void send_to_vofa_callback(void* parameter)
{
    static uint32_t time_sync_sount=0;
    v_5v0 = (gt_adc_val[0]/4095.0f)*3.3f*2.0f;     //V
    i_5v0 =  (gt_adc_val[1]/4095.0f)*3.3f/(0.12f*50)*1000;  //ma

    sample_voltage[sample_count++]=i_5v0;
    if(sample_count >NUM_SAMPLES)
    {
        sample_count = 0;
    }

    turned_size = snprintf(turned_buf, sizeof(turned_buf), "5V-OUT:%.3f,%.3f,%.3f,%d\r\n", v_5v0,mean(sample_voltage,NUM_SAMPLES),i_5v0,time_sync_sount++);

    extern chry_ringbuffer_t g_uartrx;
    // 将接收到的数据写入环形缓冲区
    chry_ringbuffer_write(&g_uartrx, turned_buf, turned_size);

}
#endif

int main(void)
{
    bsp_led_init();

    rt_pin_mode(RUNNING_LED_PIN, PIN_MODE_OUTPUT);

    rt_pin_mode(RS485_RW_PIN, PIN_MODE_OUTPUT);
    //RS485高电平发送，低电平接收，默认就进入接收模式
    rt_pin_write(RS485_RW_PIN, PIN_LOW);

    power_control_switch(POWER_5V0,POWER_STATE_ON);
//    power_control_switch(POWER_PD,POWER_ON);


    while (1)
    {
		bsp_led_left_right_move();
        rt_thread_mdelay(100);
    }

//    return RT_EOK;
}

/*!
    \brief      configure USB clock
    \param[in]  none
    \param[out] none
    \retval     none
*/
void usb_rcu_config(void)
{
#ifdef USE_USB_FS
    #ifndef USE_IRC48M
        rcu_pll48m_clock_config(RCU_PLL48MSRC_PLLQ);

        rcu_ck48m_clock_config(RCU_CK48MSRC_PLL48M);
    #else
        /* enable IRC48M clock */
        rcu_osci_on(RCU_IRC48M);

        /* wait till IRC48M is ready */
        while (SUCCESS != rcu_osci_stab_wait(RCU_IRC48M)) {
        }

        rcu_ck48m_clock_config(RCU_CK48MSRC_IRC48M);
    #endif /* USE_IRC48M */
    rcu_periph_clock_enable(RCU_USBFS);
#elif defined(USE_USB_HS)
    #ifdef USE_EMBEDDED_PHY
        rcu_pll48m_clock_config(RCU_PLL48MSRC_PLLQ);
        rcu_ck48m_clock_config(RCU_CK48MSRC_PLL48M);
    #elif defined(USE_ULPI_PHY)
        rcu_periph_clock_enable(RCU_USBHSULPI);
    #endif /* USE_EMBEDDED_PHY */

    rcu_periph_clock_enable(RCU_USBHS);
#endif /* USB_USBFS */
}

/*!
    \brief      configure USB data line GPIO
    \param[in]  none
    \param[out] none
    \retval     none
*/
void usb_gpio_config(void)
{
    rcu_periph_clock_enable(RCU_SYSCFG);

#ifdef USE_USB_FS

    rcu_periph_clock_enable(RCU_GPIOA);

    /* USBFS_DM(PA11) and USBFS_DP(PA12) GPIO pin configuration */
    gpio_mode_set(GPIOA, GPIO_MODE_AF, GPIO_PUPD_NONE, GPIO_PIN_11 | GPIO_PIN_12);
    gpio_output_options_set(GPIOA, GPIO_OTYPE_PP, GPIO_OSPEED_MAX, GPIO_PIN_11 | GPIO_PIN_12);

    gpio_af_set(GPIOA, GPIO_AF_10, GPIO_PIN_11 | GPIO_PIN_12);

#elif defined(USE_USB_HS)

    #ifdef USE_ULPI_PHY
        rcu_periph_clock_enable(RCU_GPIOA);
        rcu_periph_clock_enable(RCU_GPIOB);
        rcu_periph_clock_enable(RCU_GPIOC);
        rcu_periph_clock_enable(RCU_GPIOH);
        rcu_periph_clock_enable(RCU_GPIOI);

        /* ULPI_STP(PC0) GPIO pin configuration */
        gpio_mode_set(GPIOC, GPIO_MODE_AF, GPIO_PUPD_NONE, GPIO_PIN_0);
        gpio_output_options_set(GPIOC, GPIO_OTYPE_PP, GPIO_OSPEED_MAX, GPIO_PIN_0);

        /* ULPI_CK(PA5) GPIO pin configuration */
        gpio_mode_set(GPIOA, GPIO_MODE_AF, GPIO_PUPD_NONE, GPIO_PIN_5);
        gpio_output_options_set(GPIOA, GPIO_OTYPE_PP, GPIO_OSPEED_MAX, GPIO_PIN_5);

        /* ULPI_NXT(PH4) GPIO pin configuration */
        gpio_mode_set(GPIOH, GPIO_MODE_AF, GPIO_PUPD_NONE, GPIO_PIN_4);
        gpio_output_options_set(GPIOH, GPIO_OTYPE_PP, GPIO_OSPEED_MAX, GPIO_PIN_4);

        /* ULPI_DIR(PI11) GPIO pin configuration */
        gpio_mode_set(GPIOI, GPIO_MODE_AF, GPIO_PUPD_NONE, GPIO_PIN_11);
        gpio_output_options_set(GPIOI, GPIO_OTYPE_PP, GPIO_OSPEED_MAX, GPIO_PIN_11);

        /* ULPI_D1(PB0), ULPI_D2(PB1), ULPI_D3(PB10), ULPI_D4(PB11) \
           ULPI_D5(PB12), ULPI_D6(PB13) and ULPI_D7(PB5) GPIO pin configuration */
        gpio_mode_set(GPIOB, GPIO_MODE_AF, GPIO_PUPD_NONE, \
                        GPIO_PIN_5 | GPIO_PIN_13 | GPIO_PIN_12 |\
                        GPIO_PIN_11 | GPIO_PIN_10 | GPIO_PIN_1 | GPIO_PIN_0);
        gpio_output_options_set(GPIOB, GPIO_OTYPE_PP, GPIO_OSPEED_MAX, \
                        GPIO_PIN_5 | GPIO_PIN_13 | GPIO_PIN_12 |\
                        GPIO_PIN_11 | GPIO_PIN_10 | GPIO_PIN_1 | GPIO_PIN_0);

        /* ULPI_D0(PA3) GPIO pin configuration */
        gpio_mode_set(GPIOA, GPIO_MODE_AF, GPIO_PUPD_NONE, GPIO_PIN_3);
        gpio_output_options_set(GPIOA, GPIO_OTYPE_PP, GPIO_OSPEED_MAX, GPIO_PIN_3);

        gpio_af_set(GPIOC, GPIO_AF_10, GPIO_PIN_0);
        gpio_af_set(GPIOH, GPIO_AF_10, GPIO_PIN_4);
        gpio_af_set(GPIOI, GPIO_AF_10, GPIO_PIN_11);
        gpio_af_set(GPIOA, GPIO_AF_10, GPIO_PIN_5 | GPIO_PIN_3);
        gpio_af_set(GPIOB, GPIO_AF_10, GPIO_PIN_5 | GPIO_PIN_13 | GPIO_PIN_12 |\
                                       GPIO_PIN_11 | GPIO_PIN_10 | GPIO_PIN_1 | GPIO_PIN_0);
    #elif defined(USE_EMBEDDED_PHY)
        rcu_periph_clock_enable(RCU_GPIOB);

        /* USBHS_DM(PB14) and USBHS_DP(PB15) GPIO pin configuration */
        gpio_mode_set(GPIOB, GPIO_MODE_AF, GPIO_PUPD_NONE, GPIO_PIN_14 | GPIO_PIN_15);
        gpio_output_options_set(GPIOB, GPIO_OTYPE_PP, GPIO_OSPEED_MAX, GPIO_PIN_14 | GPIO_PIN_15);
        gpio_af_set(GPIOB, GPIO_AF_12, GPIO_PIN_14 | GPIO_PIN_15);
    #endif /* USE_ULPI_PHY */

#endif /* USE_USBFS */
}

/*!
    \brief      configure USB interrupt
    \param[in]  none
    \param[out] none
    \retval     none
*/
void usb_intr_config(void)
{
    nvic_priority_group_set(NVIC_PRIGROUP_PRE2_SUB2);

#ifdef USE_USB_FS
    nvic_irq_enable((uint8_t)USBFS_IRQn, 1U, 0U);

    #if USBFS_LOW_POWER
        /* enable the power module clock */
        rcu_periph_clock_enable(RCU_PMU);

        /* USB wakeup EXTI line configuration */
        exti_interrupt_flag_clear(EXTI_18);
        exti_init(EXTI_18, EXTI_INTERRUPT, EXTI_TRIG_RISING);
        exti_interrupt_enable(EXTI_18);

        nvic_irq_enable((uint8_t)USBFS_WKUP_IRQn, 0U, 0U);
    #endif /* USBFS_LOW_POWER */
#elif defined(USE_USB_HS)
    nvic_irq_enable((uint8_t)USBHS_IRQn, 3U, 0U);

    #if USBHS_LOW_POWER
        /* enable the power module clock */
        rcu_periph_clock_enable(RCU_PMU);

        /* USB wakeup EXTI line configuration */
        exti_interrupt_flag_clear(EXTI_20);
        exti_init(EXTI_20, EXTI_INTERRUPT, EXTI_TRIG_RISING);
        exti_interrupt_enable(EXTI_20);

        nvic_irq_enable((uint8_t)USBHS_WKUP_IRQn, 0U, 0U);
    #endif /* USBHS_LOW_POWER */
#endif /* USE_USB_FS */

#ifdef USB_HS_DEDICATED_EP1_ENABLED
    nvic_irq_enable(USBHS_EP1_Out_IRQn, 1, 0);
    nvic_irq_enable(USBHS_EP1_In_IRQn, 1, 0);
#endif /* USB_HS_DEDICATED_EP1_ENABLED */
}


void usb_dc_low_level_init(void)
{
    usb_gpio_config();
    usb_rcu_config();
    usb_intr_config();
}



#define LOG_TAG     "example"     // 该模块对应的标签。不定义时，默认：NO_TAG
#define LOG_LVL     LOG_LVL_DBG   // 该模块对应的日志输出级别。不定义时，默认：调试级别
#include <ulog.h>                 // 必须在 LOG_TAG 与 LOG_LVL 下面


/* ulog测试 */
static int __ulog_test(int argc,char **argv)
{
  static int count=0;
  static uint8_t buf[128];
  int i=0;

  for(i=0;i<sizeof(buf);i++)
  {
    buf[i]=i+count;
  }

  /* output different level log by LOG_X API */
  LOG_D("LOG_D(%d): RT-Thread is an open source IoT operating system from China.", count);
  LOG_I("LOG_I(%d): RT-Thread is an open source IoT operating system from China.", count);
  LOG_W("LOG_W(%d): RT-Thread is an open source IoT operating system from China.", count);
  LOG_E("LOG_E(%d): RT-Thread is an open source IoT operating system from China.", count);
  LOG_RAW("LOG_RAW(%d): RT-Thread is an open source IoT operating system from China.", count);
  ulog_hexdump("buf_dump_test",16,buf,sizeof(buf));

  count++;

  return 0;
}
/* 导出到 msh 命令列表中 */
MSH_CMD_EXPORT_ALIAS(__ulog_test,ulogtest, ulog test.);



