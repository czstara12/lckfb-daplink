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
 * @brief 初始化运行负载 LED 指示器。
 *
 * 该接口会初始化 PB2，并注册 RT-Thread 调度钩子，用系统 CPU time 接口统计
 * idle 线程和排除线程的运行时间。
 *
 * @return RT_EOK 表示初始化成功，其他值表示已经初始化或系统 CPU time 不可用。
 */
rt_err_t runtime_load_indicator_start(void);

/**
 * @brief 运行一次负载指示状态机。
 *
 * 应在主循环中周期调用。该接口会按 1 秒窗口更新负载，并根据负载调整 PB2
 * 的翻转周期。
 */
void runtime_load_indicator_process(void);

/**
 * @brief 获取最近一次估算的 CPU 运行负载。
 *
 * @return 最近一次采样窗口内的负载百分比，范围 0 到 100。
 */
rt_uint8_t runtime_load_indicator_get_load(void);

#endif /* __RUNTIME_LOAD_INDICATOR_H__ */
