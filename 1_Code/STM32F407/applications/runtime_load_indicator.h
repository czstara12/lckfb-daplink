/*
 * SPDX-License-Identifier: MIT
 * Origin: Created for LCKFB-DAPLINK-DEBUG-TOOL runtime load indication.
 * Created-By: gpt-5.5
 * Signed-off-by: czstara12
 */

#ifndef __RUNTIME_LOAD_INDICATOR_H__
#define __RUNTIME_LOAD_INDICATOR_H__

#include <rtthread.h>

/**
 * @brief 启动运行负载 LED 指示器。
 *
 * 该接口会注册 RT-Thread idle hook，并启动后台线程按空闲计数估算 CPU 负载。
 * LED 闪烁频率会随负载升高而加快。
 *
 * @return RT_EOK 表示启动成功，其他值表示启动失败或已经启动。
 */
rt_err_t runtime_load_indicator_start(void);

/**
 * @brief 获取最近一次估算的 CPU 运行负载。
 *
 * @return 最近一次采样窗口内的负载百分比，范围 0 到 100。
 */
rt_uint8_t runtime_load_indicator_get_load(void);

#endif /* __RUNTIME_LOAD_INDICATOR_H__ */
