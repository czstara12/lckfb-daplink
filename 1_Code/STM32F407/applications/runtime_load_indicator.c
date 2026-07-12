/*
 * SPDX-License-Identifier: MIT
 * Origin: Created for LCKFB-DAPLINK-DEBUG-TOOL runtime load indication.
 * Created-By: gpt-5.5
 * Signed-off-by: czstara12
 */

#include "runtime_load_indicator.h"

#include <drv_gpio.h>

#define LOAD_INDICATOR_PIN             GET_PIN(B, 2)
#define LOAD_SAMPLE_PERIOD_MS          1000U
#define LOAD_THREAD_POLL_MS            20U
#define LOAD_THREAD_STACK_SIZE         768U
#define LOAD_THREAD_PRIORITY           19U
#define LOAD_THREAD_TICK               10U
#define LOAD_MIN_BLINK_PERIOD_MS       120U
#define LOAD_MAX_BLINK_PERIOD_MS       1000U

static volatile rt_uint32_t idle_count;
static rt_uint32_t idle_reference;
static rt_uint32_t last_idle_count;
static rt_uint8_t current_load;
static struct rt_thread indicator_thread_control
    __attribute__((section(".ccm.cpu"), aligned(8)));
static rt_uint8_t indicator_thread_stack[LOAD_THREAD_STACK_SIZE]
    __attribute__((section(".ccm.cpu"), aligned(8)));
static rt_thread_t indicator_thread;
static rt_bool_t indicator_started;
static rt_base_t indicator_level = PIN_LOW;

static void runtime_load_idle_hook(void)
{
    idle_count++;
}

static rt_uint16_t runtime_load_to_blink_period(rt_uint8_t load)
{
    rt_uint32_t span = LOAD_MAX_BLINK_PERIOD_MS - LOAD_MIN_BLINK_PERIOD_MS;

    return (rt_uint16_t)(LOAD_MAX_BLINK_PERIOD_MS - (span * load) / 100U);
}

static void runtime_load_sample(void)
{
    rt_uint32_t idle_now = idle_count;
    rt_uint32_t idle_delta = idle_now - last_idle_count;
    rt_uint32_t load = 0U;

    last_idle_count = idle_now;

    if (idle_delta > idle_reference)
    {
        idle_reference = idle_delta;
    }

    if (idle_reference > 0U)
    {
        if (idle_delta < idle_reference)
        {
            load = ((idle_reference - idle_delta) * 100U) / idle_reference;
        }

        if (load > 100U)
        {
            load = 100U;
        }
    }

    current_load = (rt_uint8_t)load;
}

static void runtime_load_indicator_entry(void *parameter)
{
    rt_uint32_t sample_elapsed = 0U;
    rt_uint32_t blink_elapsed = 0U;
    rt_uint16_t blink_period = LOAD_MAX_BLINK_PERIOD_MS;

    RT_UNUSED(parameter);

    rt_pin_write(LOAD_INDICATOR_PIN, indicator_level);

    while (1)
    {
        rt_thread_mdelay(LOAD_THREAD_POLL_MS);

        sample_elapsed += LOAD_THREAD_POLL_MS;
        blink_elapsed += LOAD_THREAD_POLL_MS;

        if (sample_elapsed >= LOAD_SAMPLE_PERIOD_MS)
        {
            sample_elapsed = 0U;
            runtime_load_sample();
            blink_period = runtime_load_to_blink_period(current_load);
        }

        if (blink_elapsed >= blink_period)
        {
            blink_elapsed = 0U;
            indicator_level = (indicator_level == PIN_LOW) ? PIN_HIGH : PIN_LOW;
            rt_pin_write(LOAD_INDICATOR_PIN, indicator_level);
        }
    }
}

rt_err_t runtime_load_indicator_start(void)
{
    rt_err_t ret;

    if (indicator_started == RT_TRUE)
    {
        return -RT_EBUSY;
    }

    rt_pin_mode(LOAD_INDICATOR_PIN, PIN_MODE_OUTPUT);
    rt_pin_write(LOAD_INDICATOR_PIN, indicator_level);

    last_idle_count = idle_count;

    ret = rt_thread_idle_sethook(runtime_load_idle_hook);
    if (ret != RT_EOK)
    {
        return ret;
    }

    indicator_thread = &indicator_thread_control;
    ret = rt_thread_init(indicator_thread,
                         "loadled",
                         runtime_load_indicator_entry,
                         RT_NULL,
                         indicator_thread_stack,
                         sizeof(indicator_thread_stack),
                         LOAD_THREAD_PRIORITY,
                         LOAD_THREAD_TICK);
    if (ret != RT_EOK)
    {
        rt_thread_idle_delhook(runtime_load_idle_hook);
        indicator_thread = RT_NULL;
        return ret;
    }

    ret = rt_thread_startup(indicator_thread);
    if (ret != RT_EOK)
    {
        rt_thread_detach(indicator_thread);
        rt_thread_idle_delhook(runtime_load_idle_hook);
        indicator_thread = RT_NULL;
        return ret;
    }

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

    rt_kprintf("runtime load: %u%%\n", runtime_load_indicator_get_load());
}
MSH_CMD_EXPORT(runtime_load, show runtime load percentage);
#endif
