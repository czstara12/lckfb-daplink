/*
 * SPDX-License-Identifier: MIT
 * Origin: copied from build/f4usb25n01 test project
 * Created-By: gpt-5.5
 *
 * 署名: czstara12
 */

#ifndef BOARD_PORTS_W25N01GV_H__
#define BOARD_PORTS_W25N01GV_H__

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define W25N01GV_JEDEC_ID_LEN        3U
#define W25N01GV_MF_ID_WINBOND       0xEFU
#define W25N01GV_DEVICE_ID_0         0xAAU
#define W25N01GV_DEVICE_ID_1         0x21U
#define W25N01GV_PAGE_SIZE           2048U
#define W25N01GV_OOB_SIZE            64U
#define W25N01GV_PAGES_PER_BLOCK     64U
#define W25N01GV_BLOCK_COUNT         1024U
#define W25N01GV_BLOCK_SIZE          (W25N01GV_PAGE_SIZE * W25N01GV_PAGES_PER_BLOCK)
#define W25N01GV_CAPACITY            (W25N01GV_BLOCK_SIZE * W25N01GV_BLOCK_COUNT)
#define W25N01GV_UFFS_BLOCK_FIRST    0U
#define W25N01GV_UFFS_BLOCK_COUNT    W25N01GV_BLOCK_COUNT
#define W25N01GV_UFFS_MTD_NAME       "w25nand"

/**
 * @brief W25N01GV 片内 ECC 状态。
 */
enum w25n01gv_ecc_status
{
    W25N01GV_ECC_OK = 0,
    W25N01GV_ECC_CORRECTED,
    W25N01GV_ECC_FAILED,
    W25N01GV_ECC_RESERVED,
};

/**
 * @brief W25N01GV 底层 I/O 统计。
 */
struct w25n01gv_io_stats
{
    uint32_t load_pages;
    uint32_t program_pages;
    uint32_t erase_blocks;
    uint32_t cache_read_bytes;
    uint32_t program_load_bytes;
    uint32_t busy_wait_us;
};

/**
 * @brief 判断 JEDEC ID 是否匹配 Winbond W25N01GV。
 *
 * @param id 由 W25N01GV_JEDEC_ID_LEN 个字节组成的 JEDEC ID 缓冲区。
 *
 * @return 匹配返回 1，否则返回 0。
 */
int w25n01gv_match_jedec_id(const uint8_t id[W25N01GV_JEDEC_ID_LEN]);

/**
 * @brief 将块号和块内页号转换为 W25N01GV row address。
 *
 * @param block 块号。
 * @param page 块内页号。
 *
 * @return 可用于 0x13/0x10 等指令的 row address。
 */
uint32_t w25n01gv_row_from_block_page(uint16_t block, uint8_t page);

/**
 * @brief 从状态寄存器值中解析 ECC 状态位。
 *
 * @param status 通过 0x0F/0xC0 读取到的状态寄存器值。
 *
 * @return 解析后的 ECC 状态。
 */
enum w25n01gv_ecc_status w25n01gv_decode_ecc_status(uint8_t status);

/**
 * @brief 填充用于 W25N01GV 页写入验证的确定性测试数据。
 *
 * @param buf 输出缓冲区。
 * @param len 输出长度。
 * @param block 测试块号。
 * @param page 块内页号。
 */
void w25n01gv_fill_test_pattern(uint8_t *buf, uint32_t len, uint16_t block, uint8_t page);

/**
 * @brief 校验缓冲区是否符合 W25N01GV 测试数据 pattern。
 *
 * @param buf 输入缓冲区。
 * @param len 输入长度。
 * @param block 测试块号。
 * @param page 块内页号。
 *
 * @return 匹配返回 1，否则返回 0。
 */
int w25n01gv_check_test_pattern(const uint8_t *buf, uint32_t len, uint16_t block, uint8_t page);

/**
 * @brief 清零 W25N01GV 底层 I/O 统计。
 */
void w25n01gv_stats_reset(void);

/**
 * @brief 获取 W25N01GV 底层 I/O 统计。
 *
 * @param stats 输出统计。
 */
void w25n01gv_stats_get(struct w25n01gv_io_stats *stats);

#ifdef __cplusplus
}
#endif

#endif /* BOARD_PORTS_W25N01GV_H__ */
