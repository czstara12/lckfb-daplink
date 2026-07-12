/*
 * Copyright (c) 2023, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2023-07-06     Supperthomas first version
 */


#ifndef __BOARD_H__
#define __BOARD_H__

#include <stm32f4xx.h>

#ifdef __cplusplus
extern "C" {
#endif

#define STM32_SRAM_SIZE        (128)
#define STM32_SRAM_END         (0x20000000 + STM32_SRAM_SIZE * 1024)

#define STM32_FLASH_START_ADRESS     ((uint32_t)0x08000000)
#define STM32_FLASH_SIZE             (512 * 1024)
#define STM32_FLASH_END_ADDRESS      ((uint32_t)(STM32_FLASH_START_ADRESS + STM32_FLASH_SIZE))

#define SPI1_DMA_RX_IRQHandler       DMA2_Stream2_IRQHandler
#define SPI1_RX_DMA_RCC              RCC_AHB1ENR_DMA2EN
#define SPI1_RX_DMA_INSTANCE         DMA2_Stream2
#define SPI1_RX_DMA_CHANNEL          DMA_CHANNEL_3
#define SPI1_RX_DMA_IRQ              DMA2_Stream2_IRQn

#define SPI1_DMA_TX_IRQHandler       DMA2_Stream5_IRQHandler
#define SPI1_TX_DMA_RCC              RCC_AHB1ENR_DMA2EN
#define SPI1_TX_DMA_INSTANCE         DMA2_Stream5
#define SPI1_TX_DMA_CHANNEL          DMA_CHANNEL_3
#define SPI1_TX_DMA_IRQ              DMA2_Stream5_IRQn

#if defined(__ARMCC_VERSION)
extern int Image$$RW_IRAM1$$ZI$$Limit;
#define HEAP_BEGIN      ((void *)&Image$$RW_IRAM1$$ZI$$Limit)
#elif __ICCARM__
#pragma section="CSTACK"
#define HEAP_BEGIN      (__segment_end("CSTACK"))
#else
extern int __bss_end;
#define HEAP_BEGIN      ((void *)&__bss_end)
#endif

#define HEAP_END        STM32_SRAM_END

void SystemClock_Config(void);

#ifdef __cplusplus
}
#endif

#endif
