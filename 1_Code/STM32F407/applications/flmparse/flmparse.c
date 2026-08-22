/*
 * SPDX-License-Identifier: MIT
 * Origin: https://gitee.com/jhembedded/flmparse
 * Created-By: xcwynya
 */

#include <dfs_posix.h>
#include <fcntl.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <unistd.h>

#include <rtthread.h>

#include "elf.h"
#include "FlashOS.h"
#include "flmparse.h"

#define LOG_TAG "flmparse"
#define LOG_LVL LOG_LVL_DBG
#include <ulog.h>

#define FLM_BLOB_MAX_SIZE (10U * 1024U)
#define LOAD_FUN_NUM 5U

static int read_data_from_file(const char *file_name, uint32_t offset, void *buffer, uint32_t size)
{
    int fd;
    ssize_t read_bytes;

    fd = open(file_name, O_RDONLY | O_BINARY);
    if (fd < 0)
    {
        LOG_E("open file failed: %s", file_name);
        return -1;
    }

    if (lseek(fd, offset, SEEK_SET) < 0)
    {
        close(fd);
        return -2;
    }

    read_bytes = read(fd, buffer, size);
    close(fd);
    return read_bytes == (ssize_t)size ? 0 : -3;
}

static int resize_buffer(uint8_t **buffer, uint32_t size)
{
    uint8_t *new_buffer;

    if (size == 0U)
    {
        return -1;
    }

    new_buffer = *buffer == RT_NULL ? rt_malloc(size) : rt_realloc(*buffer, size);
    if (new_buffer == RT_NULL)
    {
        return -1;
    }

    *buffer = new_buffer;
    return 0;
}

static int parse_flm_sections(const char *file_name, flm_image_t *image)
{
    static const char *const function_names[LOAD_FUN_NUM] = {
        "Init", "UnInit", "EraseChip", "EraseSector", "ProgramPage"
    };
    int function_name_offsets[LOAD_FUN_NUM] = {-1, -1, -1, -1, -1};
    uint8_t *buffer = RT_NULL;
    Elf32_Ehdr elf_header = {0};
    Elf32_Shdr symbol_header = {0};
    Elf32_Shdr string_header = {0};
    uint32_t i;
    uint32_t k;
    uint32_t found_mask = 0U;
    int result = -1;

    if (read_data_from_file(file_name, 0, &elf_header, sizeof(elf_header)) < 0 ||
        rt_memcmp(elf_header.e_ident, ELFMAG, SELFMAG) != 0)
    {
        goto cleanup;
    }

    if (resize_buffer(&buffer, sizeof(Elf32_Phdr) * elf_header.e_phnum) < 0 ||
        read_data_from_file(file_name, elf_header.e_phoff, buffer,
                            sizeof(Elf32_Phdr) * elf_header.e_phnum) < 0)
    {
        goto cleanup;
    }

    for (i = 0; i < elf_header.e_phnum; i++)
    {
        const Elf32_Phdr *program_header = &((const Elf32_Phdr *)buffer)[i];

        if (program_header->p_type == PT_LOAD &&
            (program_header->p_flags & (PF_X | PF_W | PF_R)) == (PF_X | PF_W | PF_R))
        {
            if (program_header->p_filesz > FLM_BLOB_MAX_SIZE - 32U)
            {
                result = -2;
                goto cleanup;
            }
            if (read_data_from_file(file_name, program_header->p_offset,
                                    &image->blob[8], program_header->p_filesz) < 0)
            {
                result = -3;
                goto cleanup;
            }
            image->blob_size = program_header->p_filesz + 32U;
        }
        else if (program_header->p_type == PT_LOAD && i == 1U)
        {
            if (program_header->p_filesz < offsetof(FlashDevice_T, szPage) + sizeof(uint32_t) ||
                read_data_from_file(file_name,
                                    program_header->p_offset + offsetof(FlashDevice_T, devAdr),
                                    &image->device_address, sizeof(image->device_address)) < 0 ||
                read_data_from_file(file_name,
                                    program_header->p_offset + offsetof(FlashDevice_T, szPage),
                                    &image->page_size, sizeof(image->page_size)) < 0)
            {
                result = -3;
                goto cleanup;
            }
        }
    }

    if (image->blob_size == 0U || image->page_size == 0U)
    {
        goto cleanup;
    }

    if (resize_buffer(&buffer, sizeof(Elf32_Shdr) * elf_header.e_shnum) < 0 ||
        read_data_from_file(file_name, elf_header.e_shoff, buffer,
                            sizeof(Elf32_Shdr) * elf_header.e_shnum) < 0)
    {
        goto cleanup;
    }

    for (i = 0; i < elf_header.e_shnum; i++)
    {
        const Elf32_Shdr *section_headers = (const Elf32_Shdr *)buffer;

        if (section_headers[i].sh_type == SHT_SYMTAB &&
            section_headers[i].sh_link < elf_header.e_shnum &&
            section_headers[section_headers[i].sh_link].sh_type == SHT_STRTAB)
        {
            symbol_header = section_headers[i];
            string_header = section_headers[section_headers[i].sh_link];
            break;
        }
    }

    if (string_header.sh_size == 0U || string_header.sh_size == UINT32_MAX)
    {
        result = -4;
        goto cleanup;
    }

    if (resize_buffer(&buffer, string_header.sh_size + 1U) < 0 ||
        read_data_from_file(file_name, string_header.sh_offset,
                            buffer, string_header.sh_size) < 0)
    {
        goto cleanup;
    }

    for (i = 0; i < string_header.sh_size; i++)
    {
        if (buffer[i] == '\0')
        {
            buffer[i] = '\n';
        }
    }
    buffer[string_header.sh_size] = '\0';

    for (i = 0; i < LOAD_FUN_NUM; i++)
    {
        char *name = rt_strstr((const char *)buffer, function_names[i]);

        if (name != RT_NULL)
        {
            function_name_offsets[i] = name - (char *)buffer;
        }
    }

    if (resize_buffer(&buffer, symbol_header.sh_size) < 0 ||
        read_data_from_file(file_name, symbol_header.sh_offset,
                            buffer, symbol_header.sh_size) < 0)
    {
        goto cleanup;
    }

    for (i = 0; i < symbol_header.sh_size / sizeof(Elf32_Sym); i++)
    {
        const Elf32_Sym *symbol = &((const Elf32_Sym *)buffer)[i];

        for (k = 0; k < LOAD_FUN_NUM; k++)
        {
            if (function_name_offsets[k] < 0 ||
                (uint32_t)function_name_offsets[k] != symbol->st_name)
            {
                continue;
            }

            switch (k)
            {
            case 0:
                image->init = symbol->st_value;
                break;
            case 1:
                image->uninit = symbol->st_value;
                break;
            case 2:
                image->erase_chip = symbol->st_value;
                break;
            case 3:
                image->erase_sector = symbol->st_value;
                break;
            case 4:
                image->program_page = symbol->st_value;
                break;
            default:
                break;
            }
            found_mask |= 1UL << k;
        }
    }

    result = found_mask == (1UL << LOAD_FUN_NUM) - 1UL ? 0 : -4;

cleanup:
    rt_free(buffer);
    return result;
}

int flm_parse_file(const char *file_path, flm_image_t *image)
{
    static const uint32_t halt_code[8] = {
        0xE00ABE00, 0x062D780D, 0x24084068, 0xD3000040,
        0x1E644058, 0x1C49D1FA, 0x2A001E52, 0x4770D1F2
    };
    uint32_t *resized_blob;

    if (file_path == RT_NULL || image == RT_NULL)
    {
        return -1;
    }

    rt_memset(image, 0, sizeof(*image));
    image->blob = rt_malloc(FLM_BLOB_MAX_SIZE);
    if (image->blob == RT_NULL)
    {
        LOG_E("FLM blob memory allocation failed");
        return -1;
    }

    rt_memcpy(image->blob, halt_code, sizeof(halt_code));
    if (parse_flm_sections(file_path, image) < 0)
    {
        LOG_E("Failed to parse FLM file: %s", file_path);
        flm_image_release(image);
        return -1;
    }

    resized_blob = rt_realloc(image->blob, image->blob_size);
    if (resized_blob != RT_NULL)
    {
        image->blob = resized_blob;
    }

    return 0;
}

void flm_image_release(flm_image_t *image)
{
    if (image == RT_NULL)
    {
        return;
    }

    rt_free(image->blob);
    rt_memset(image, 0, sizeof(*image));
}

/**
 * @brief 将 FLM 文件转换为可粘贴的算法定义。
 *
 * @param argc 参数数量。
 * @param argv 参数列表，第二项为 FLM 文件路径。
 * @return 始终返回 0。
 */
int parse_flm_file(int argc, char *argv[])
{
    flm_image_t image = {0};
    uint32_t i;

    if (argc != 2 || flm_parse_file(argv[1], &image) < 0)
    {
        rt_kprintf("Usage: parse_flm_file [filename]\n");
        return 0;
    }

    rt_kprintf("\nstatic const uint32_t flash_code[] =\n{");
    for (i = 0; i < image.blob_size / sizeof(uint32_t); i++)
    {
        if (i % 8U == 0U)
        {
            rt_kprintf("\n    ");
        }
        rt_kprintf("0X%08X,", image.blob[i]);
    }
    rt_kprintf("\n};\n");
    rt_kprintf("\nconst program_target_t flash_algo =\n{\n");
    rt_kprintf("    0X20000020 + 0X%08X,  // Init\n", image.init);
    rt_kprintf("    0X20000020 + 0X%08X,  // UnInit\n", image.uninit);
    rt_kprintf("    0X20000020 + 0X%08X,  // EraseChip\n", image.erase_chip);
    rt_kprintf("    0X20000020 + 0X%08X,  // EraseSector\n", image.erase_sector);
    rt_kprintf("    0X20000020 + 0X%08X,  // ProgramPage\n", image.program_page);
    rt_kprintf("    {\n");
    rt_kprintf("        0X20000001,\n");
    rt_kprintf("        0X20000C00,\n");
    rt_kprintf("        0X20001000,\n");
    rt_kprintf("    },\n");
    rt_kprintf("    0x20000400,\n");
    rt_kprintf("    0x20000000,\n");
    rt_kprintf("    sizeof(flash_code),\n");
    rt_kprintf("    flash_code,\n");
    rt_kprintf("    0x00000400,\n");
    rt_kprintf("};\n");

    flm_image_release(&image);
    return 0;
}
MSH_CMD_EXPORT(parse_flm_file, parse_flm_file);
