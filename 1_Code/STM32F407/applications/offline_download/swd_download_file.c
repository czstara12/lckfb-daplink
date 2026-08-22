/*
* 梁山派软硬件资料与相关扩展板软硬件资料官网全部开源
* 开发板官网：www.lckfb.com
* 技术支持常驻论坛，任何技术问题欢迎随时交流学习
* 立创论坛：club.szlcsc.com
* 关注bilibili账号：【立创开发板】，掌握我们的最新动态！
* 不靠卖板赚钱，以培养中国工程师为己任
* Change Logs:
* Date           Author       Notes
* 2024-03-21     LCKFB-yzh    first version
 */

#include <rtthread.h>
#include <dfs_posix.h>
#include <unistd.h>

#define error_t daplink_error_t
#include "SWD_flash.h"
#undef error_t
#include "swd_host.h"
#include "debug_cm.h"
//#include "lv_conf.h"
#define LV_FILE_EXPLORER_PATH_MAX_LEN 256

#include "swd_download_file.h"
#include "FlashOS.h"
#include "flmparse.h"

#define LOG_TAG     "swd_download_file"     // 该模块对应的标签。不定义时，默认：NO_TAG
#define LOG_LVL     LOG_LVL_DBG   // 该模块对应的日志输出级别。不定义时，默认：调试级别
#include <ulog.h>                 // 必须在 LOG_TAG 与 LOG_LVL 下面

struct offline_download_info_t offline_download_info;

char choose_device_path[LV_FILE_EXPLORER_PATH_MAX_LEN];
char choose_firmware_bin_path[LV_FILE_EXPLORER_PATH_MAX_LEN];

//判断当前是不是正在脱机下载中
static int8_t on_offline_swd_downloading = 0;

static flm_image_t flash_image;

int8_t is_on_offline_swd_downloading(void)
{
    return on_offline_swd_downloading;
}

static void start_offline_swd_download(void)
{
    on_offline_swd_downloading = 1;
}

static void stop_offline_swd_download(void)
{
    on_offline_swd_downloading = 0;
}

program_target_t flash_algo = {
    0X20000020 + 0X0000001D, // Init
    0X20000020 + 0X0000004F, // UnInit
    0X20000020 + 0X0000005D, // EraseChip
    0X20000020 + 0X00000089, // EraseSector
    0X20000020 + 0X000000D5, // ProgramPage

    // BKPT : start of blob + 1
    // RSB  : address to access global/static data
    // RSP  : stack pointer
    {
        0X20000001,
        0X20000C00,
        0X20001000,
    },

    0x20000400, // mem buffer location
    0x20000000, // location to write prog_blob in target RAM
    0,          // prog_blob size
    0,          // address of prog_blob
    0x00000400, // ram_to_flash_bytes_to_be_written
};

static void release_flash_image(void)
{
    flm_image_release(&flash_image);
    flash_algo.algo_blob = RT_NULL;
    flash_algo.algo_size = 0U;
}

//uint32_t Flash_Sect_Size = 8 * 1024;
//uint32_t Flash_Page_Size = 4 * 1024;
uint32_t Flash_Start_Addr = 0x00000000;

int8_t swd_download_update_flash_algo(char *_file_path)
{
    release_flash_image();
    if (flm_parse_file(_file_path, &flash_image) < 0)
    {
        LOG_E("parse flm from file error");
        return -1;
    }
    flash_algo.init = flash_image.init + 0X20000020;
    flash_algo.uninit = flash_image.uninit + 0X20000020;
    flash_algo.erase_chip = flash_image.erase_chip + 0X20000020;
    flash_algo.erase_sector = flash_image.erase_sector + 0X20000020;
    flash_algo.program_page = flash_image.program_page + 0X20000020;

    // flash_algo.sys_call_s.breakpoint = 0x20000001;
    // flash_algo.sys_call_s.static_base = 0x20000C00;
    // flash_algo.sys_call_s.stack_pointer = 0x20001000;

    flash_algo.program_buffer = 0x20000000 + flash_image.blob_size;
    flash_algo.algo_start = 0x20000000;

    flash_algo.algo_size = flash_image.blob_size;
    flash_algo.algo_blob = flash_image.blob;

    flash_algo.program_buffer_size = flash_image.page_size;
	
	flash_algo.sys_call_s.breakpoint = 0x20000000 + 1;
    flash_algo.sys_call_s.static_base = flash_algo.program_buffer + flash_algo.program_buffer_size;
	
	Flash_Start_Addr = flash_image.device_address;
	return 0;
}

int32_t swd_download_from_file(char *_file_path)
{
    int32_t fd = -1, read_size = 0, ret = 0;
    static char file_path[LV_FILE_EXPLORER_PATH_MAX_LEN] = {0};
    struct stat file_stat;
    uint8_t *download_buffer = RT_NULL;
    uint8_t *file_read_buf;
    uint8_t *target_mcu_flash_read_buf;
    uint32_t val;

    start_offline_swd_download();

    if (flash_image.blob == RT_NULL || flash_image.page_size == 0U ||
        flash_image.page_size > UINT32_MAX / 2U)
    {
        LOG_E("flash algorithm is not ready");
        goto error;
    }

    download_buffer = rt_calloc(2U, flash_image.page_size);
    if (download_buffer == RT_NULL)
    {
        LOG_E("download buffer memory allocation failed");
        goto error;
    }
    file_read_buf = download_buffer;
    target_mcu_flash_read_buf = download_buffer + flash_image.page_size;

    if (rt_strlen(_file_path) < sizeof(file_path))
    {
        rt_strncpy(file_path, _file_path, sizeof(file_path));
    }
    else
    {
        LOG_E("file path too long");
        rt_snprintf( offline_download_info.info_message, sizeof( offline_download_info.info_message), "%s", "path too long");
        goto error;
    }

    // 1 从TF卡中获取文件

    //判断这个文件后缀是否为bin
    if (rt_strstr(file_path, ".bin") == NULL)
    {
        LOG_E("only support .bin file");
        rt_snprintf( offline_download_info.info_message, sizeof( offline_download_info.info_message), "%s", "only .bin file");
        goto error;
    }
    /* 以只读模式打开文件*/
    fd = open(file_path, O_RDONLY);
    if (fd < 0)
    {
        LOG_E("open file failed");
        rt_snprintf( offline_download_info.info_message, sizeof( offline_download_info.info_message), "%s", "open file faile");
        goto error;
    }
    else
    {
        LOG_I("open file success");
        ret = stat(file_path, &file_stat);
        if (ret == 0)
            LOG_I("%s file size = %d", file_path,file_stat.st_size);
        else
        {
            LOG_E("%s file not fonud",file_path);
            goto error;
        }
    }
    if (file_stat.st_size <= 0)
    {
        LOG_E("firmware file is empty");
        goto error;
    }

    // 2 将FLM文件加载到目标单片机的RAM里
    swd_init_debug();

    swd_set_target_state_hw(RESET_PROGRAM);

    swd_read_dp(0x00, &val);
    LOG_I("the chip IDCODE: %08X", val);
	
	//梁山派GD32F470需要再此处加入延时  而小华HC32F4A0不能有这个延时  还不清楚根因
	//TODO:考虑如何适配未来更多的芯片
	if (rt_strstr(choose_device_path, "HC32") == NULL)
    {
		rt_thread_mdelay(200);
	}else
	{
		;
	}
	
    rt_snprintf( offline_download_info.info_message, sizeof( offline_download_info.info_message), "%s", "load FLM file");
    if(target_flash_init(Flash_Start_Addr) != ERROR_SUCCESS)
    {
        LOG_E("target flash init error");
        goto error;
    }

    // 3 擦除目标单片机的Flash
    rt_snprintf( offline_download_info.info_message, sizeof( offline_download_info.info_message), "%s", "erase flash");
    for (uint32_t addr = 0; addr < file_stat.st_size; addr += flash_image.page_size)
    {
        target_flash_erase_sector(Flash_Start_Addr + addr);
        offline_download_info.progress = (addr * 100) / file_stat.st_size;
        LOG_D("erase sector is %08X", Flash_Start_Addr + addr);
    }

    // 4 读取目标flash是否为已经擦除成功
//    for (uint32_t addr = 0; addr < file_stat.st_size; addr += 1024)
//    {
//        swd_read_memory(Flash_Start_Addr + addr, file_read_buf, 1024);
////        ulog_hexdump("target_flash", 8,file_read_buf,sizeof(file_read_buf));
//    }
    // 5 下载至目标单片机的Flash
    rt_snprintf( offline_download_info.info_message, sizeof( offline_download_info.info_message), "%s", "download flash");
    for (uint32_t addr = 0; addr < file_stat.st_size; addr += flash_image.page_size)
    {
        uint32_t transfer_size = file_stat.st_size - addr > flash_image.page_size
                                     ? flash_image.page_size
                                     : file_stat.st_size - addr;

        rt_memset(file_read_buf, 0xFF, flash_image.page_size);
        read_size = read(fd, file_read_buf, transfer_size);
        if (read_size != (int32_t)transfer_size)
        {
            LOG_E("firmware file read failed");
            goto error;
        }

		LOG_I("read size = %d", read_size);
        offline_download_info.progress = (addr * 100) / file_stat.st_size;
        target_flash_program_page(Flash_Start_Addr + addr, file_read_buf,
                                  flash_image.page_size);
    }
    close(fd);
    fd = -1;
    fd = open(file_path, O_RDONLY); //重新打开文件要从头开始读
    if (fd < 0)
    {
        goto error;
    }
    // 6 读回校验
    rt_snprintf( offline_download_info.info_message, sizeof( offline_download_info.info_message), "%s", "verify flash");
    for (uint32_t addr = 0; addr < file_stat.st_size; addr += flash_image.page_size)
    {
        uint32_t transfer_size = file_stat.st_size - addr > flash_image.page_size
                                     ? flash_image.page_size
                                     : file_stat.st_size - addr;

        swd_read_memory(Flash_Start_Addr + addr, target_mcu_flash_read_buf, flash_image.page_size);

        read_size = read(fd, file_read_buf, transfer_size);
        if (read_size != (int32_t)transfer_size)
        {
            LOG_E("firmware verify read failed");
            goto error;
        }

        LOG_I("file read size = %d", read_size);
        offline_download_info.progress = (addr * 100) / file_stat.st_size;
        // ulog_hexdump("target_flash", 8,target_mcu_flash_read_buf,sizeof(target_mcu_flash_read_buf));
        //
        // ulog_hexdump("file", 8,file_read_buf,sizeof(file_read_buf));

        if (rt_memcmp(target_mcu_flash_read_buf, file_read_buf, transfer_size) == 0)
        {
            LOG_I("verify pass");
        }
        else
        {
            LOG_E("verify fail");
            rt_snprintf( offline_download_info.info_message, sizeof( offline_download_info.info_message), "%s", "verify fail");
            goto error;
        }
    }
    offline_download_info.progress = 100;
    rt_snprintf( offline_download_info.info_message, sizeof( offline_download_info.info_message), "%s", "verify pass");
    close(fd);
    fd = -1;
    //7 让目标单片机开始运行
    soft_reset_target();

    offline_download_info.success_download_count += 1;

    rt_free(download_buffer);
    release_flash_image();
    stop_offline_swd_download();

    return 0;
    // 6 进入错误状态
error:
    if (fd >= 0)
    {
        close(fd);
    }
    rt_free(download_buffer);
    release_flash_image();
    LOG_E("error occured!");
    rt_snprintf( offline_download_info.info_message, sizeof( offline_download_info.info_message), "%s", "error occured!");

    stop_offline_swd_download();
    return -1;
}

uint16_t get_offline_success_download_count(void)
{
    return offline_download_info.success_download_count;
}

uint16_t get_offline_download_progress(void)
{
    return offline_download_info.progress;
}

char* get_offline_download_info_message(void)
{
    return offline_download_info.info_message;
}


static int _offline_download_info_init(void)
{
    rt_snprintf(choose_device_path, sizeof(choose_device_path),
                "%s", "/sdcard/MCU/ST/STM32F4xx/STM32F4xx_512.FLM");
    rt_snprintf(choose_firmware_bin_path, sizeof(choose_firmware_bin_path),
                "%s", "/sdcard/firmware/f407_100ms.bin");
    offline_download_info.success_download_count = 0;
    rt_snprintf( offline_download_info.info_message, sizeof( offline_download_info.info_message), "%s", "wait for start");
    offline_download_info.progress = 0;
    return 0;
}
INIT_APP_EXPORT(_offline_download_info_init);


//extern void buzzer_beep_set(uint16_t _tone_freq, uint8_t _volume);

//void swd_download_100ms(void)
//{
//    swd_download_update_flash_algo("/sdcard/MCU/ST/STM32F4xx/STM32F4xx_512.FLM");
//    if(swd_download_from_file("/sdcard/firmware/f407_100ms.bin")==-1)
//    {
//        //下载错误
//        buzzer_beep_set(4000,100);
//        rt_thread_mdelay(50);
//        buzzer_beep_set(0,0);
//        rt_thread_mdelay(50);
//        buzzer_beep_set(4000,100);
//        rt_thread_mdelay(50);
//        buzzer_beep_set(0,0);
//        rt_thread_mdelay(50);
//        buzzer_beep_set(4000,100);
//        rt_thread_mdelay(50);
//        buzzer_beep_set(0,0);
//        rt_thread_mdelay(50);
//    }
//    else
//    {
//        //下载成功
//        buzzer_beep_set(4000,100);
//        rt_thread_mdelay(50);
//        buzzer_beep_set(0,0);
//        rt_thread_mdelay(50);
//    }
//}

//MSH_CMD_EXPORT(swd_download_100ms, swd_download_100ms);

//void swd_download_500ms(void)
//{
//    swd_download_update_flash_algo("/sdcard/MCU/ST/STM32F4xx/STM32F4xx_512.FLM");
//    if(swd_download_from_file("/sdcard/firmware/f407_500ms.bin")==-1)
//    {
//        //下载错误
//        buzzer_beep_set(4000,100);
//        rt_thread_mdelay(50);
//        buzzer_beep_set(0,0);
//        rt_thread_mdelay(50);
//        buzzer_beep_set(4000,100);
//        rt_thread_mdelay(50);
//        buzzer_beep_set(0,0);
//        rt_thread_mdelay(50);
//        buzzer_beep_set(4000,100);
//        rt_thread_mdelay(50);
//        buzzer_beep_set(0,0);
//        rt_thread_mdelay(50);
//    }
//    else
//    {
//        //下载成功
//        buzzer_beep_set(4000,100);
//        rt_thread_mdelay(50);
//        buzzer_beep_set(0,0);
//        rt_thread_mdelay(50);
//    }
//}

//MSH_CMD_EXPORT(swd_download_500ms, swd_download_500ms);

void swd_download_rtthread(void)
{
    if (swd_download_update_flash_algo("/sdcard/MCU/ST/STM32F4xx/STM32F4xx_512.FLM") == 0)
    {
        swd_download_from_file("/sdcard/firmware/rtthread.bin");
    }
}

MSH_CMD_EXPORT(swd_download_rtthread, swd_download_rtthread);
