/*
 * SPDX-License-Identifier: MIT
 * Origin: Created for LCKFB-DAPLINK-DEBUG-TOOL runtime load indication.
 * Created-By: gpt-5.5
 * Signed-off-by: czstara12
 */

#include "runtime_load_indicator.h"

#include <drv_gpio.h>
#include <rthw.h>

#define LOAD_INDICATOR_PIN             GET_PIN(B, 2)
#define LOAD_EXCLUDE_THREAD_NAME       "dap_loop"
#define LOAD_SAMPLE_PERIOD_MS          1000U
#define LOAD_MIN_BLINK_PERIOD_MS       120U
#define LOAD_MAX_BLINK_PERIOD_MS       1000U
#define LOAD_THREAD_STAT_MAX           24U
#define LOAD_THREAD_LIST_MAX           24U

struct runtime_thread_stat
{
    rt_thread_t thread;
    rt_uint32_t cycle;
    rt_uint32_t last_cycle;
};

static rt_thread_t idle_thread;
static rt_thread_t exclude_thread;
static rt_uint32_t last_switch_cycle;
static rt_uint32_t sample_start_cycle;
static rt_uint32_t nonload_cycle_sum;
static rt_uint32_t last_total_cycle;
static rt_uint32_t last_nonload_cycle;
static rt_tick_t sample_start_tick;
static rt_tick_t blink_start_tick;
static rt_uint16_t blink_period = LOAD_MAX_BLINK_PERIOD_MS;
static rt_uint8_t current_load;
static rt_bool_t indicator_started;
static rt_base_t indicator_level = PIN_LOW;
static struct runtime_thread_stat thread_stats[LOAD_THREAD_STAT_MAX];

static rt_uint16_t runtime_load_to_blink_period(rt_uint8_t load)
{
    rt_uint32_t span = LOAD_MAX_BLINK_PERIOD_MS - LOAD_MIN_BLINK_PERIOD_MS;

    return (rt_uint16_t)(LOAD_MAX_BLINK_PERIOD_MS - (span * load) / 100U);
}

static rt_uint32_t runtime_load_get_cycle(void)
{
    return (rt_uint32_t)clock_cpu_gettime();
}

static struct runtime_thread_stat *runtime_load_find_thread_stat(rt_thread_t thread)
{
    rt_uint32_t i;
    struct runtime_thread_stat *empty = RT_NULL;

    for (i = 0U; i < LOAD_THREAD_STAT_MAX; i++)
    {
        if (thread_stats[i].thread == thread)
        {
            return &thread_stats[i];
        }

        if ((empty == RT_NULL) && (thread_stats[i].thread == RT_NULL))
        {
            empty = &thread_stats[i];
        }
    }

    if (empty != RT_NULL)
    {
        empty->thread = thread;
        empty->cycle = 0U;
        empty->last_cycle = 0U;
    }

    return empty;
}

static void runtime_load_add_thread_cycle(rt_thread_t thread, rt_uint32_t cycle)
{
    struct runtime_thread_stat *stat;

    if ((thread == RT_NULL) || (cycle == 0U))
    {
        return;
    }

    stat = runtime_load_find_thread_stat(thread);
    if (stat != RT_NULL)
    {
        stat->cycle += cycle;
    }
}

static void runtime_load_scheduler_hook(rt_thread_t from, rt_thread_t to)
{
    rt_uint32_t now_cycle = runtime_load_get_cycle();
    rt_uint32_t delta_cycle = now_cycle - last_switch_cycle;

    if ((from == idle_thread) || (from == exclude_thread))
    {
        nonload_cycle_sum += delta_cycle;
    }

    runtime_load_add_thread_cycle(from, delta_cycle);

    last_switch_cycle = now_cycle;
    RT_UNUSED(to);
}

static void runtime_load_sample(void)
{
    rt_base_t level;
    rt_uint32_t now_cycle;
    rt_uint32_t total_cycle;
    rt_uint32_t nonload_cycle;
    rt_thread_t self_thread;
    rt_uint32_t current_delta;
    rt_uint32_t i;
    rt_uint32_t load = 0U;

    level = rt_hw_interrupt_disable();
    now_cycle = runtime_load_get_cycle();
    total_cycle = now_cycle - sample_start_cycle;
    self_thread = rt_thread_self();
    current_delta = now_cycle - last_switch_cycle;

    if ((self_thread == idle_thread) || (self_thread == exclude_thread))
    {
        nonload_cycle_sum += current_delta;
    }

    runtime_load_add_thread_cycle(self_thread, current_delta);

    nonload_cycle = nonload_cycle_sum;
    nonload_cycle_sum = 0U;
    last_switch_cycle = now_cycle;

    for (i = 0U; i < LOAD_THREAD_STAT_MAX; i++)
    {
        thread_stats[i].last_cycle = thread_stats[i].cycle;
        thread_stats[i].cycle = 0U;
    }

    rt_hw_interrupt_enable(level);

    if (total_cycle > 0U)
    {
        if (nonload_cycle < total_cycle)
        {
            load = 100U - (rt_uint32_t)(((rt_uint64_t)nonload_cycle * 100U) / total_cycle);
        }

        if (load > 100U)
        {
            load = 100U;
        }
    }

    current_load = (rt_uint8_t)load;
    last_total_cycle = total_cycle;
    last_nonload_cycle = nonload_cycle;
    blink_period = runtime_load_to_blink_period(current_load);
    sample_start_cycle = now_cycle;
}

void runtime_load_indicator_process(void)
{
    rt_tick_t now_tick;

    if (indicator_started != RT_TRUE)
    {
        return;
    }

    now_tick = rt_tick_get();

    if (exclude_thread == RT_NULL)
    {
        exclude_thread = rt_thread_find(LOAD_EXCLUDE_THREAD_NAME);
    }

    if ((now_tick - sample_start_tick) >= rt_tick_from_millisecond(LOAD_SAMPLE_PERIOD_MS))
    {
        runtime_load_sample();
        sample_start_tick = now_tick;
    }

    if ((now_tick - blink_start_tick) >= rt_tick_from_millisecond(blink_period))
    {
        blink_start_tick = now_tick;
        indicator_level = (indicator_level == PIN_LOW) ? PIN_HIGH : PIN_LOW;
        rt_pin_write(LOAD_INDICATOR_PIN, indicator_level);
    }
}

rt_err_t runtime_load_indicator_start(void)
{
    if (indicator_started == RT_TRUE)
    {
        return -RT_EBUSY;
    }

    if (clock_cpu_getres() == 0U)
    {
        return -RT_ENOSYS;
    }

    rt_pin_mode(LOAD_INDICATOR_PIN, PIN_MODE_OUTPUT);
    rt_pin_write(LOAD_INDICATOR_PIN, indicator_level);

    idle_thread = rt_thread_idle_gethandler();
    exclude_thread = rt_thread_find(LOAD_EXCLUDE_THREAD_NAME);
    last_switch_cycle = runtime_load_get_cycle();
    sample_start_cycle = last_switch_cycle;
    sample_start_tick = rt_tick_get();
    blink_start_tick = sample_start_tick;
    rt_scheduler_sethook(runtime_load_scheduler_hook);
    indicator_started = RT_TRUE;

    return RT_EOK;
}

rt_uint8_t runtime_load_indicator_get_load(void)
{
    return current_load;
}

#ifdef FINSH_USING_MSH
static void runtime_load(int argc, char **argv)
{
    RT_UNUSED(argc);
    RT_UNUSED(argv);

    rt_kprintf("runtime load: %u%%, total=%u, nonload=%u\n",
               runtime_load_indicator_get_load(),
               last_total_cycle,
               last_nonload_cycle);
    if (idle_thread != RT_NULL)
    {
        rt_kprintf("idle thread: %.*s, exclude: %.*s, self: %.*s\n",
                   RT_NAME_MAX,
                   idle_thread->parent.name,
                   RT_NAME_MAX,
                   exclude_thread != RT_NULL ? exclude_thread->parent.name : "none",
                   RT_NAME_MAX,
                   rt_thread_self()->parent.name);
    }
}
MSH_CMD_EXPORT(runtime_load, show runtime load percentage);

static rt_uint32_t runtime_load_get_thread_last_cycle(rt_thread_t thread)
{
    rt_uint32_t i;

    for (i = 0U; i < LOAD_THREAD_STAT_MAX; i++)
    {
        if (thread_stats[i].thread == thread)
        {
            return thread_stats[i].last_cycle;
        }
    }

    return 0U;
}

static void thread_load(int argc, char **argv)
{
    rt_object_t threads[LOAD_THREAD_LIST_MAX];
    rt_uint32_t cycles[LOAD_THREAD_LIST_MAX];
    rt_uint32_t total_cycle;
    rt_uint32_t i;
    rt_base_t level;
    int count;

    RT_UNUSED(argc);
    RT_UNUSED(argv);

    count = rt_object_get_pointers(RT_Object_Class_Thread,
                                   threads,
                                   LOAD_THREAD_LIST_MAX);
    total_cycle = last_total_cycle;

    level = rt_hw_interrupt_disable();
    for (i = 0U; (i < (rt_uint32_t)count) && (i < LOAD_THREAD_LIST_MAX); i++)
    {
        cycles[i] = runtime_load_get_thread_last_cycle((rt_thread_t)threads[i]);
    }
    rt_hw_interrupt_enable(level);

    rt_kprintf("thread   cycle      usage\n");
    rt_kprintf("-------- ---------- -----\n");

    for (i = 0U; (i < (rt_uint32_t)count) && (i < LOAD_THREAD_LIST_MAX); i++)
    {
        rt_uint32_t usage = 0U;
        rt_thread_t thread = (rt_thread_t)threads[i];

        if (total_cycle > 0U)
        {
            usage = (rt_uint32_t)(((rt_uint64_t)cycles[i] * 100U) / total_cycle);
            if (usage > 100U)
            {
                usage = 100U;
            }
        }

        rt_kprintf("%-*.*s %10u %3u%%\n",
                   RT_NAME_MAX,
                   RT_NAME_MAX,
                   thread->parent.name,
                   cycles[i],
                   usage);
    }
}
MSH_CMD_EXPORT(thread_load, show thread CPU usage in latest sample window);
#endif
