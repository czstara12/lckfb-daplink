/*
 * SPDX-License-Identifier: MIT
 * Origin: Derived from the existing FLM parser module
 * Created-By: xcwynya
 */

#ifndef FLMPARSE_H
#define FLMPARSE_H

#include <stdint.h>

/**
 * @brief FLM 文件解析结果。
 */
typedef struct
{
    uint32_t *blob;
    uint32_t blob_size;
    uint32_t init;
    uint32_t uninit;
    uint32_t erase_chip;
    uint32_t erase_sector;
    uint32_t program_page;
    uint32_t device_address;
    uint32_t page_size;
} flm_image_t;

/**
 * @brief 解析 FLM 文件并按需分配算法数据。
 *
 * @param file_path FLM 文件路径。
 * @param image 解析结果，使用完毕后必须调用 flm_image_release()。
 * @return 0 表示成功，负值表示解析失败。
 */
int flm_parse_file(const char *file_path, flm_image_t *image);

/**
 * @brief 释放 FLM 解析结果占用的内存。
 *
 * @param image 待释放的解析结果。
 */
void flm_image_release(flm_image_t *image);

#endif /* FLMPARSE_H */
