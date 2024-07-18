/*
 * Copyright (c) 2006-2021, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author        Notes
 * 2021-10-18     Meco Man      First version
 */

#ifndef LV_CONF_H
#define LV_CONF_H

#include <rtconfig.h>

#define LV_COLOR_DEPTH          16
#define LV_USE_PERF_MONITOR     0
#define MY_DISP_HOR_RES          240
#define MY_DISP_VER_RES          240
#define LV_USE_LOG              1

#define LV_COLOR_16_SWAP 1

#ifdef PKG_USING_LV_MUSIC_DEMO
/* music player demo */
#define LV_HOR_RES_MAX              MY_DISP_HOR_RES
#define LV_VER_RES_MAX              MY_DISP_VER_RES
#define LV_USE_DEMO_RTT_MUSIC       1
#define LV_DEMO_RTT_MUSIC_AUTO_PLAY 1
#define LV_FONT_MONTSERRAT_12       1
#define LV_FONT_MONTSERRAT_16       1
#define LV_COLOR_SCREEN_TRANSP      1

#endif

#define LV_USE_FS_POSIX        1

#define LV_USE_BMP             1
#define LV_FS_POSIX_PATH       "/"
#define LV_FS_POSIX_LETTER     'S'
#define LV_FS_POSIX_CACHE_SIZE 0

//#define LV_USE_DEMO_BENCHMARK       1


//#define LV_USE_DEMO_WIDGETS         1


//#define LV_USE_DEMO_MUSIC           1

//#define LV_USE_FILE_EXPLORER 1
#define LV_FILE_EXPLORER_PATH_MAX_LEN 256

#define LV_FONT_MONTSERRAT_12 1
#define LV_FONT_MONTSERRAT_18 0


#endif
