/*
 * SPDX-License-Identifier: Apache-2.0
 * Origin: Derived from RT-Thread UFFS DFS adapter
 * Created-By: xcwynya
 *
 * Copyright (c) 2006-2018, RT-Thread Development Team
 *
 * Change Logs:
 * Date           Author       Notes
 * 2011-10-22     prife        the first version
 * 2012-03-28     prife        use mtd device interface
 * 2012-04-05     prife        update uffs with official repo and use uffs_UnMount/Mount
 * 2017-04-12     lizhen9880   fix the f_bsize and f_blocks issue in function dfs_uffs_statfs
 * 2026-08-09     xcwynya      adapt to current DFS v1 API
 */

#include <rtthread.h>

#include <dfs_fs.h>
#include <dfs_file.h>
#include <rtdevice.h>

#include "dfs_uffs.h"

#ifdef st_atime
#undef st_atime
#endif
#ifdef st_mtime
#undef st_mtime
#endif
#ifdef st_ctime
#undef st_ctime
#endif

#include "uffs/uffs_fd.h"
#include "uffs/uffs_mtb.h"
#include "uffs/uffs_mem.h"
#include "uffs/uffs_utils.h"

#define UFFS_DEVICE_MAX         2
#define UFFS_MOUNT_PATH_MAX     128
#define FILE_PATH_MAX           256

struct _nand_dev
{
    struct rt_mtd_nand_device *dev;
    struct uffs_StorageAttrSt storage;
    uffs_Device uffs_dev;
    uffs_MountTable mount_table;
    char mount_path[UFFS_MOUNT_PATH_MAX];
    void *data;
};

static struct _nand_dev nand_part[UFFS_DEVICE_MAX];

static int uffs_result_to_dfs(int result)
{
    int status = -1;

    result = result < 0 ? -result : result;
    switch (result)
    {
    case UENOERR:
        break;
    case UEACCES:
        status = -EINVAL;
        break;
    case UEEXIST:
        status = -EEXIST;
        break;
    case UEINVAL:
        status = -EINVAL;
        break;
    case UEMFILE:
        status = -1;
        break;
    case UENOENT:
        status = -ENOENT;
        break;
    case UETIME:
        status = -1;
        break;
    case UEBADF:
        status = -EBADF;
        break;
    case UENOMEM:
        status = -ENOSPC;
        break;
    case UEIOERR:
        status = -EIO;
        break;
    case UENOTDIR:
        status = -ENOTDIR;
        break;
    case UEISDIR:
        status = -EISDIR;
        break;
    case UEUNKNOWN_ERR:
    default:
        status = -1;
        break;
    }

    return status;
}

static URET _device_init(uffs_Device *dev)
{
    dev->attr->_private = RT_NULL;
    dev->ops = (struct uffs_FlashOpsSt *)&nand_ops;

    return U_SUCC;
}

static URET _device_release(uffs_Device *dev)
{
    RT_UNUSED(dev);
    return U_SUCC;
}

static int init_uffs_fs(struct _nand_dev *nand)
{
    uffs_MountTable *mtb;
    struct rt_mtd_nand_device *mtd;
    struct uffs_StorageAttrSt *flash_storage;

    mtb = &nand->mount_table;
    mtd = nand->dev;
    flash_storage = &nand->storage;

    uffs_setup_storage(flash_storage, mtd);

    if (mtb->dev)
    {
#if CONFIG_USE_SYSTEM_MEMORY_ALLOCATOR > 0
        uffs_MemSetupSystemAllocator(&mtb->dev->mem);
#endif
        mtb->dev->Init = _device_init;
        mtb->dev->Release = _device_release;
        mtb->dev->attr = flash_storage;

        uffs_RegisterMountTable(mtb);
    }

    return uffs_Mount(nand->mount_path) == U_SUCC ? 0 : -1;
}

static int dfs_uffs_mount(struct dfs_filesystem *fs,
                          unsigned long rwflag,
                          const void *data)
{
    rt_base_t index;
    uffs_MountTable *mount_part;
    struct rt_mtd_nand_device *dev;

    RT_UNUSED(rwflag);
    RT_UNUSED(data);
    RT_ASSERT(rt_strlen(fs->path) < (UFFS_MOUNT_PATH_MAX - 1));
    dev = RT_MTD_NAND_DEVICE(fs->dev_id);

    for (index = 0; index < UFFS_DEVICE_MAX; index++)
    {
        if (nand_part[index].dev == RT_NULL)
        {
            break;
        }
    }
    if (index == UFFS_DEVICE_MAX)
    {
        return -ENOENT;
    }

    nand_part[index].dev = dev;
    rt_snprintf(nand_part[index].mount_path, UFFS_MOUNT_PATH_MAX, "%s/", fs->path);
    if (nand_part[index].mount_path[1] == '/')
    {
        nand_part[index].mount_path[1] = 0;
    }

    mount_part = &nand_part[index].mount_table;
    mount_part->mount = nand_part[index].mount_path;
    mount_part->dev = &nand_part[index].uffs_dev;
    rt_memset(mount_part->dev, 0, sizeof(uffs_Device));
    mount_part->dev->_private = dev;
    mount_part->start_block = dev->block_start;
    mount_part->end_block = dev->block_end;

    if (init_uffs_fs(&nand_part[index]) < 0)
    {
        return uffs_result_to_dfs(uffs_get_error());
    }

    return 0;
}

static int dfs_uffs_unmount(struct dfs_filesystem *fs)
{
    rt_base_t index;
    int result;

    for (index = 0; index < UFFS_DEVICE_MAX; index++)
    {
        if (nand_part[index].dev == RT_MTD_NAND_DEVICE(fs->dev_id))
        {
            nand_part[index].dev = RT_NULL;
            result = uffs_UnMount(nand_part[index].mount_path);
            if (result != U_SUCC)
            {
                break;
            }

            result = uffs_UnRegisterMountTable(&nand_part[index].mount_table);
            return result == U_SUCC ? RT_EOK : -1;
        }
    }

    return -ENOENT;
}

static int dfs_uffs_mkfs(rt_device_t dev_id, const char *fs_name)
{
    rt_base_t index;
    rt_uint32_t block;
    struct rt_mtd_nand_device *mtd;
    rt_bool_t mounted = RT_FALSE;

    RT_UNUSED(fs_name);

    for (index = 0; index < UFFS_DEVICE_MAX; index++)
    {
        if (nand_part[index].dev == (struct rt_mtd_nand_device *)dev_id)
        {
            break;
        }
    }

    mtd = RT_MTD_NAND_DEVICE(dev_id);
    if (mtd == RT_NULL)
    {
        return -ENODEV;
    }

    if (index != UFFS_DEVICE_MAX)
    {
        mounted = RT_TRUE;
        uffs_UnMount(nand_part[index].mount_path);
        mtd = nand_part[index].dev;
    }

    for (block = mtd->block_start; block <= mtd->block_end; block++)
    {
        rt_mtd_nand_erase_block(mtd, block);
#if defined(RT_UFFS_USE_CHECK_MARK_FUNCITON)
        if (rt_mtd_nand_check_block(mtd, block) != RT_EOK)
        {
            rt_kprintf("found bad block %d\n", block);
            rt_mtd_nand_mark_badblock(mtd, block);
        }
#endif
    }

    if (mounted && init_uffs_fs(&nand_part[index]) < 0)
    {
        return uffs_result_to_dfs(uffs_get_error());
    }

    return RT_EOK;
}

static int dfs_uffs_statfs(struct dfs_filesystem *fs, struct statfs *buf)
{
    rt_base_t index;
    struct rt_mtd_nand_device *mtd = RT_MTD_NAND_DEVICE(fs->dev_id);

    RT_ASSERT(mtd != RT_NULL);

    for (index = 0; index < UFFS_DEVICE_MAX; index++)
    {
        if (nand_part[index].dev == (void *)mtd)
        {
            break;
        }
    }
    if (index == UFFS_DEVICE_MAX)
    {
        return -ENOENT;
    }

    buf->f_bsize = mtd->page_size * mtd->pages_per_block;
    buf->f_blocks = mtd->block_end - mtd->block_start + 1;
    buf->f_bfree = uffs_GetDeviceFree(&nand_part[index].uffs_dev) / buf->f_bsize;

    return 0;
}

static int dfs_uffs_open(struct dfs_file *file)
{
    int fd;
    int oflag;
    int mode;
    char *file_path;

    oflag = file->flags;
    if (oflag & O_DIRECTORY)
    {
        uffs_DIR *dir;

        if ((oflag & O_CREAT) && uffs_mkdir(file->vnode->path) < 0)
        {
            return uffs_result_to_dfs(uffs_get_error());
        }

        file_path = rt_malloc(FILE_PATH_MAX);
        if (file_path == RT_NULL)
        {
            return -ENOMEM;
        }

        if (file->vnode->path[0] == '/' && file->vnode->path[1] != 0)
        {
            rt_snprintf(file_path, FILE_PATH_MAX, "%s/", file->vnode->path);
        }
        else
        {
            file_path[0] = '/';
            file_path[1] = 0;
        }

        dir = uffs_opendir(file_path);
        rt_free(file_path);
        if (dir == RT_NULL)
        {
            return uffs_result_to_dfs(uffs_get_error());
        }

        file->data = dir;
        return RT_EOK;
    }

    mode = 0;
    if (oflag & O_RDONLY) mode |= UO_RDONLY;
    if (oflag & O_WRONLY) mode |= UO_WRONLY;
    if (oflag & O_RDWR) mode |= UO_RDWR;
    if (oflag & O_CREAT) mode |= UO_CREATE;
    if (oflag & O_TRUNC) mode |= UO_TRUNC;
    if (oflag & O_EXCL) mode |= UO_EXCL;

    fd = uffs_open(file->vnode->path, mode);
    if (fd < 0)
    {
        return uffs_result_to_dfs(uffs_get_error());
    }

    file->data = (void *)fd;
    file->pos = uffs_seek(fd, 0, USEEK_CUR);
    file->vnode->size = uffs_seek(fd, 0, USEEK_END);
    uffs_seek(fd, file->pos, USEEK_SET);

    if (oflag & O_APPEND)
    {
        file->pos = uffs_seek(fd, 0, USEEK_END);
    }

    return 0;
}

static int dfs_uffs_close(struct dfs_file *file)
{
    if (file->flags & O_DIRECTORY)
    {
        if (uffs_closedir((uffs_DIR *)file->data) < 0)
        {
            return uffs_result_to_dfs(uffs_get_error());
        }
        return 0;
    }

    if (uffs_close((int)file->data) == 0)
    {
        return 0;
    }

    return uffs_result_to_dfs(uffs_get_error());
}

static int dfs_uffs_ioctl(struct dfs_file *file, int cmd, void *args)
{
    RT_UNUSED(file);
    RT_UNUSED(cmd);
    RT_UNUSED(args);
    return -ENOSYS;
}

static ssize_t dfs_uffs_read(struct dfs_file *file, void *buf, size_t len)
{
    int char_read = uffs_read((int)file->data, buf, len);

    if (char_read < 0)
    {
        return uffs_result_to_dfs(uffs_get_error());
    }

    file->pos = uffs_seek((int)file->data, 0, USEEK_CUR);
    return char_read;
}

static ssize_t dfs_uffs_write(struct dfs_file *file, const void *buf, size_t len)
{
    int char_write = uffs_write((int)file->data, buf, len);

    if (char_write < 0)
    {
        return uffs_result_to_dfs(uffs_get_error());
    }

    file->pos = uffs_seek((int)file->data, 0, USEEK_CUR);
    return char_write;
}

static int dfs_uffs_flush(struct dfs_file *file)
{
    if (uffs_flush((int)file->data) < 0)
    {
        return uffs_result_to_dfs(uffs_get_error());
    }
    return 0;
}

static int uffs_seekdir(uffs_DIR *dir, long offset)
{
    int i = 0;

    while (i < offset)
    {
        if (uffs_readdir(dir) == RT_NULL)
        {
            return -1;
        }
        i++;
    }

    return 0;
}

static off_t dfs_uffs_seek(struct dfs_file *file, off_t offset)
{
    int result;

    if (file->vnode->type == FT_DIRECTORY)
    {
        uffs_rewinddir((uffs_DIR *)file->data);
        result = uffs_seekdir((uffs_DIR *)file->data, offset / sizeof(struct dirent));
    }
    else if (file->vnode->type == FT_REGULAR)
    {
        result = uffs_seek((int)file->data, offset, USEEK_SET);
    }
    else
    {
        return -EINVAL;
    }

    if (result >= 0)
    {
        file->pos = offset;
        return offset;
    }

    return uffs_result_to_dfs(uffs_get_error());
}

static int dfs_uffs_getdents(struct dfs_file *file,
                             struct dirent *dirp,
                             uint32_t count)
{
    rt_uint32_t index = 0;
    char *file_path;
    uffs_DIR *dir = (uffs_DIR *)file->data;

    RT_ASSERT(dir != RT_NULL);
    count = (count / sizeof(struct dirent)) * sizeof(struct dirent);
    if (count == 0)
    {
        return -EINVAL;
    }

    file_path = rt_malloc(FILE_PATH_MAX);
    if (file_path == RT_NULL)
    {
        return -ENOMEM;
    }

    while (index * sizeof(struct dirent) < count)
    {
        struct uffs_stat stat;
        struct dirent *entry = dirp + index;
        struct uffs_dirent *uffs_entry = uffs_readdir(dir);

        if (uffs_entry == RT_NULL)
        {
            break;
        }

        if (file->vnode->path[0] == '/' && file->vnode->path[1] != 0)
        {
            rt_snprintf(file_path, FILE_PATH_MAX, "%s/%s",
                        file->vnode->path, uffs_entry->d_name);
        }
        else
        {
            rt_strncpy(file_path, uffs_entry->d_name, FILE_PATH_MAX);
        }

        uffs_stat(file_path, &stat);
        switch (stat.st_mode & US_IFMT)
        {
        case US_IFREG:
            entry->d_type = DT_REG;
            break;
        case US_IFDIR:
            entry->d_type = DT_DIR;
            break;
        default:
            entry->d_type = DT_UNKNOWN;
            break;
        }

        entry->d_namlen = rt_strlen(uffs_entry->d_name);
        entry->d_reclen = (rt_uint16_t)sizeof(struct dirent);
        rt_strncpy(entry->d_name, uffs_entry->d_name, entry->d_namlen + 1);
        index++;
    }

    rt_free(file_path);
    file->pos += index * sizeof(struct dirent);

    if (index == 0 && uffs_get_error() != UENOERR)
    {
        return uffs_result_to_dfs(uffs_get_error());
    }

    return index * sizeof(struct dirent);
}

static int dfs_uffs_unlink(struct dfs_filesystem *fs, const char *path)
{
    int result;
    struct uffs_stat stat;

    RT_UNUSED(fs);
    if (uffs_lstat(path, &stat) < 0)
    {
        return uffs_result_to_dfs(uffs_get_error());
    }

    switch (stat.st_mode & US_IFMT)
    {
    case US_IFREG:
        result = uffs_remove(path);
        break;
    case US_IFDIR:
        result = uffs_rmdir(path);
        break;
    default:
        return -EINVAL;
    }

    return result < 0 ? uffs_result_to_dfs(uffs_get_error()) : 0;
}

static int dfs_uffs_rename(struct dfs_filesystem *fs,
                           const char *oldpath,
                           const char *newpath)
{
    RT_UNUSED(fs);
    if (uffs_rename(oldpath, newpath) < 0)
    {
        return uffs_result_to_dfs(uffs_get_error());
    }
    return 0;
}

static int dfs_uffs_stat(struct dfs_filesystem *fs,
                         const char *path,
                         struct stat *buf)
{
    struct uffs_stat stat;

    if (rt_strcmp(path, fs->path) == 0)
    {
        rt_memset(buf, 0, sizeof(*buf));
        buf->st_mode = S_IFDIR | S_IRUSR | S_IXUSR | S_IRGRP | S_IXGRP |
                       S_IROTH | S_IXOTH;
        return 0;
    }

    if (uffs_stat(path, &stat) < 0)
    {
        return uffs_result_to_dfs(uffs_get_error());
    }

    buf->st_dev = 0;
    buf->st_mode = stat.st_mode;
    buf->st_size = stat.st_size;
    buf->st_mtim.tv_sec = stat.st_mtime;
    buf->st_mtim.tv_nsec = 0;

    return 0;
}

static const struct dfs_file_ops dfs_uffs_fops =
{
    dfs_uffs_open,
    dfs_uffs_close,
    dfs_uffs_ioctl,
    dfs_uffs_read,
    dfs_uffs_write,
    dfs_uffs_flush,
    dfs_uffs_seek,
    dfs_uffs_getdents,
};

static const struct dfs_filesystem_ops dfs_uffs_ops =
{
    "uffs",
    DFS_FS_FLAG_FULLPATH,
    &dfs_uffs_fops,
    dfs_uffs_mount,
    dfs_uffs_unmount,
    dfs_uffs_mkfs,
    dfs_uffs_statfs,
    dfs_uffs_unlink,
    dfs_uffs_stat,
    dfs_uffs_rename,
};

/**
 * @brief 注册 UFFS 文件系统到 DFS v1。
 *
 * @return 成功返回 RT_EOK，初始化失败返回 -RT_ERROR。
 */
int dfs_uffs_init(void)
{
    dfs_register(&dfs_uffs_ops);

    if (uffs_InitObjectBuf() == U_SUCC && uffs_DirEntryBufInit() == U_SUCC)
    {
        uffs_InitGlobalFsLock();
        return RT_EOK;
    }

    return -RT_ERROR;
}
INIT_COMPONENT_EXPORT(dfs_uffs_init);
