/**
 * @file drv_filesystem.c
 * @brief 挂载 ROMFS、SD 卡和 W25Q32 LittleFS。
 * Copyright (c) 2006-2021, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2018-12-13     balanceTWK   add sdcard port file
 * 2021-05-10     Meco Man     fix a bug that cannot use fatfs in the main thread at starting up
 * 2021-07-28     Meco Man     implement romfs as the root filesystem
 */

#include <rtthread.h>
#include <dfs_romfs.h>
#include <dfs_fs.h>
#include <dfs_file.h>
#ifdef BSP_USING_FLASH_LITTLEFS
#include <fal.h>
#endif

#if DFS_FILESYSTEMS_MAX < 4
#error "Please define DFS_FILESYSTEMS_MAX more than 4"
#endif
#if DFS_FILESYSTEM_TYPES_MAX < 4
#error "Please define DFS_FILESYSTEM_TYPES_MAX more than 4"
#endif

#define DBG_TAG "app.filesystem"
#define DBG_LVL DBG_ERROR
#include <rtdbg.h>

#ifdef BSP_USING_FS_AUTO_MOUNT
#ifdef BSP_USING_SDCARD_FATFS
static int onboard_sdcard_mount(void)
{
    if (dfs_mount("sd0", "/sdcard", "elm", 0, 0) == RT_EOK)
    {
        LOG_I("SD card mount to '/sdcard'");
    }
    else
    {
        LOG_E("SD card mount to '/sdcard' failed!");
    }

    return RT_EOK;
}
#endif /* BSP_USING_SDCARD_FATFS */
#endif /* BSP_USING_FS_AUTO_MOUNT */

#ifdef BSP_USING_FLASH_FS_AUTO_MOUNT
#ifdef BSP_USING_FLASH_LITTLEFS
#define FS_PARTITION_NAME "filesystem"

/**
 * @brief 将整片 W25Q32 的 LittleFS 挂载到 /fal，失败时保留数据。
 * @return 成功返回 RT_EOK，初始化或挂载失败返回负值。
 */
static int onboard_fal_mount(void)
{
    struct rt_device *flash_dev;

    if (fal_init() <= 0)
    {
        LOG_E("FAL initialization failed");
        return -RT_ERROR;
    }
    flash_dev = fal_mtd_nor_device_create(FS_PARTITION_NAME);
    if (flash_dev == RT_NULL)
    {
        LOG_E("Can't create an MTD NOR device on '%s' partition.", FS_PARTITION_NAME);
        return -RT_ERROR;
    }

    if (dfs_mount(flash_dev->parent.name, "/fal", "lfs", 0, 0) == RT_EOK)
    {
        LOG_I("W25Q32 LittleFS mount to '/fal'");
    }
    else
    {
        LOG_E("LittleFS mount failed; initialize with: mkfs -t lfs filesystem");
        return -RT_ERROR;
    }

    return RT_EOK;
}
#endif /* BSP_USING_FLASH_LITTLEFS */
#endif /* BSP_USING_FLASH_FS_AUTO_MOUNT */


const struct romfs_dirent _romfs_root[] =
{
#ifdef BSP_USING_SDCARD_FATFS
    {ROMFS_DIRENT_DIR, "sdcard", RT_NULL, 0},
#endif

#ifdef BSP_USING_FLASH_LITTLEFS
    {ROMFS_DIRENT_DIR, "fal", RT_NULL, 0},
#endif
};

const struct romfs_dirent romfs_root =
{
    ROMFS_DIRENT_DIR, "/", (rt_uint8_t *)_romfs_root, sizeof(_romfs_root) / sizeof(_romfs_root[0])
};

/**
 * @brief 挂载板载文件系统。
 *
 * @return 成功返回 RT_EOK，挂载失败返回负值。
 */
int filesystem_mount(void)
{
#ifdef BSP_USING_FS
    if (dfs_mount(RT_NULL, "/", "rom", 0, &(romfs_root)) != 0)
    {
        LOG_E("rom mount to '/' failed!");
        return -RT_ERROR;
    }
#endif

#ifdef BSP_USING_FS_AUTO_MOUNT
    onboard_sdcard_mount();
#endif /* BSP_USING_FS_AUTO_MOUNT */

#ifdef BSP_USING_FLASH_FS_AUTO_MOUNT
    return onboard_fal_mount();
#endif

    return RT_EOK;
}
