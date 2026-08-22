/*
 * SPDX-License-Identifier: MIT
 * Origin: Created for LCKFB-DAPLINK-DEBUG-TOOL UFFS performance testing.
 * Created-By: xcwynya
 */

#include <rtthread.h>
#include <dfs_posix.h>

#include <stdlib.h>
#include <unistd.h>

#include "w25n01gv.h"

#define UFFS_SPEED_PATH              "/uffs/.speed_test.bin"
#define UFFS_SPEED_DEFAULT_SIZE_KIB  1024U
#define UFFS_SPEED_DEFAULT_BLOCK     4096U
#define UFFS_SPEED_MAX_SIZE_KIB      (64U * 1024U)
#define UFFS_SPEED_MAX_BLOCK         (64U * 1024U)

static void uffs_speed_print(const char *operation, rt_uint32_t size_kib,
                             rt_tick_t ticks)
{
    rt_uint32_t elapsed_ms;
    rt_uint32_t speed_kib_s;

    if (ticks == 0U)
    {
        ticks = 1U;
    }
    elapsed_ms = (rt_uint32_t)(((rt_uint64_t)ticks * 1000U) / RT_TICK_PER_SECOND);
    speed_kib_s = (rt_uint32_t)(((rt_uint64_t)size_kib * RT_TICK_PER_SECOND) / ticks);
    rt_kprintf("%s: %u KiB, %u ms, %u KiB/s\n",
               operation, size_kib, elapsed_ms, speed_kib_s);
}

static void uffs_speed_print_io(const char *operation,
                                const struct w25n01gv_io_stats *stats)
{
    rt_kprintf("%s io: load=%u program=%u erase=%u busy=%u ms, read=%u B, write=%u B\n",
               operation,
               stats->load_pages,
               stats->program_pages,
               stats->erase_blocks,
               stats->busy_wait_us / 1000U,
               stats->cache_read_bytes,
               stats->program_load_bytes);
}

static int uffs_speed_parse(const char *text, rt_uint32_t min,
                            rt_uint32_t max, rt_uint32_t *value)
{
    char *end;
    unsigned long parsed = strtoul(text, &end, 0);

    if (*text == '\0' || *end != '\0' || parsed < min || parsed > max)
    {
        return -RT_EINVAL;
    }
    *value = (rt_uint32_t)parsed;
    return RT_EOK;
}

/**
 * @brief 测试 UFFS 顺序写入和读取速度，并校验读取内容。
 *
 * @param argc 参数数量。
 * @param argv 参数列表：uffs_speed [size_kib] [block_bytes]。
 */
static void uffs_speed(int argc, char **argv)
{
    rt_uint32_t size_kib = UFFS_SPEED_DEFAULT_SIZE_KIB;
    rt_uint32_t block_size = UFFS_SPEED_DEFAULT_BLOCK;
    rt_uint32_t total_size;
    rt_uint32_t done;
    rt_uint8_t *buffer;
    rt_tick_t start_tick;
    rt_tick_t write_ticks;
    rt_tick_t read_ticks;
    struct w25n01gv_io_stats write_stats;
    struct w25n01gv_io_stats read_stats;
    int fd = -1;

    if (argc > 3 ||
        (argc > 1 && uffs_speed_parse(argv[1], 1U, UFFS_SPEED_MAX_SIZE_KIB,
                                     &size_kib) != RT_EOK) ||
        (argc > 2 && uffs_speed_parse(argv[2], 1U, UFFS_SPEED_MAX_BLOCK,
                                     &block_size) != RT_EOK))
    {
        rt_kprintf("usage: uffs_speed [size_kib:1..65536] [block_bytes:1..65536]\n");
        return;
    }

    total_size = size_kib * 1024U;
    if (block_size > total_size)
    {
        block_size = total_size;
    }
    buffer = rt_malloc(block_size);
    if (buffer == RT_NULL)
    {
        rt_kprintf("uffs_speed: allocate %u bytes failed\n", block_size);
        return;
    }
    rt_memset(buffer, 0xA5, block_size);
    unlink(UFFS_SPEED_PATH);

    w25n01gv_stats_reset();
    start_tick = rt_tick_get();
    fd = open(UFFS_SPEED_PATH, O_WRONLY | O_CREAT | O_TRUNC, 0);
    for (done = 0U; fd >= 0 && done < total_size;)
    {
        rt_uint32_t chunk = total_size - done > block_size
                                ? block_size
                                : total_size - done;
        int written = write(fd, buffer, chunk);

        if (written <= 0)
        {
            break;
        }
        done += (rt_uint32_t)written;
    }
    if (fd < 0 || done != total_size || fsync(fd) != 0 || close(fd) != 0)
    {
        rt_kprintf("uffs_speed: write failed at %u/%u bytes\n", done, total_size);
        goto cleanup;
    }
    fd = -1;
    write_ticks = rt_tick_get() - start_tick;
    w25n01gv_stats_get(&write_stats);

    rt_memset(buffer, 0, block_size);
    w25n01gv_stats_reset();
    start_tick = rt_tick_get();
    fd = open(UFFS_SPEED_PATH, O_RDONLY, 0);
    for (done = 0U; fd >= 0 && done < total_size;)
    {
        rt_uint32_t chunk = total_size - done > block_size
                                ? block_size
                                : total_size - done;
        int read_size = read(fd, buffer, chunk);
        rt_uint32_t index;

        if (read_size <= 0)
        {
            break;
        }
        for (index = 0U; index < (rt_uint32_t)read_size; index++)
        {
            if (buffer[index] != 0xA5U)
            {
                rt_kprintf("uffs_speed: verify failed at byte %u\n", done + index);
                goto cleanup;
            }
        }
        done += (rt_uint32_t)read_size;
    }
    if (fd < 0 || done != total_size || close(fd) != 0)
    {
        rt_kprintf("uffs_speed: read failed at %u/%u bytes\n", done, total_size);
        goto cleanup;
    }
    fd = -1;
    read_ticks = rt_tick_get() - start_tick;
    w25n01gv_stats_get(&read_stats);

    uffs_speed_print("write", size_kib, write_ticks);
    uffs_speed_print("read ", size_kib, read_ticks);
    uffs_speed_print_io("write", &write_stats);
    uffs_speed_print_io("read ", &read_stats);
    rt_kprintf("verify: passed, block: %u bytes\n", block_size);
cleanup:
    if (fd >= 0)
    {
        close(fd);
    }
    unlink(UFFS_SPEED_PATH);
    rt_free(buffer);
}
MSH_CMD_EXPORT(uffs_speed, test UFFS speed: uffs_speed [size_kib] [block_bytes]);
