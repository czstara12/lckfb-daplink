/**
 * @file fal_cfg.h
 * @brief W25Q32 整片 LittleFS 分区配置。
 * Copyright (c) 2006-2021, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2018-12-5      SummerGift   first version
 */

#ifndef _FAL_CFG_H_
#define _FAL_CFG_H_

#include <board.h>
#include <fal_def.h>

#define FLASH_SIZE_GRANULARITY_16K   (4 * 16 * 1024)
#define FLASH_SIZE_GRANULARITY_64K   (8 * 64 * 1024)
#define FLASH_SIZE_GRANULARITY_128K  (8 * 128 * 1024)
#define STM32_FLASH_START_ADRESS_16K  STM32_FLASH_START_ADRESS
#define STM32_FLASH_START_ADRESS_64K  STM32_FLASH_START_ADRESS
#define STM32_FLASH_START_ADRESS_128K STM32_FLASH_START_ADRESS

extern const struct fal_flash_dev stm32_onchip_flash_16k;
extern const struct fal_flash_dev stm32_onchip_flash_64k;
extern const struct fal_flash_dev stm32_onchip_flash_128k;
/** W25Q32 外部 SPI NOR Flash 的 FAL 设备。 */
extern struct fal_flash_dev w25q32;

/** FAL 仅注册用于 LittleFS 的 W25Q32。 */
#define FAL_FLASH_DEV_TABLE                                          \
{                                                                    \
    &w25q32,                                                         \
}

/** 整片 4 MiB 作为一个文件系统分区，无额外保留区域。 */

#define FAL_PART_TABLE                                                 \
{                                                                      \
    {FAL_PART_MAGIC_WORD, "filesystem", "W25Q32", 0, 4 * 1024 * 1024, 0}, \
}
#endif /*FAL_PART_TABLE*/
