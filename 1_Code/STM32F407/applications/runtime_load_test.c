/*
 * SPDX-License-Identifier: MIT
 * Origin: Created for LCKFB-DAPLINK-DEBUG-TOOL runtime load test.
 * Created-By: gpt-5.5
 * Signed-off-by: czstara12
 */

#include <rtthread.h>

#include <stdlib.h>

#define PI_LOAD_DEFAULT_SECONDS         10U
#define PI_LOAD_MAX_SECONDS             120U
#define PI_SCALE                        1000000LL

static void pi_load_run(rt_uint32_t seconds)
{
    rt_tick_t start_tick = rt_tick_get();
    rt_tick_t run_ticks = rt_tick_from_millisecond((rt_int32_t)(seconds * 1000U));
    rt_uint32_t index = 0U;
    rt_int64_t pi_scaled = 0;

    while ((rt_tick_get() - start_tick) < run_ticks)
    {
        rt_int64_t term = (4LL * PI_SCALE) / (rt_int64_t)(index * 2U + 1U);

        if ((index & 1U) == 0U)
        {
            pi_scaled += term;
        }
        else
        {
            pi_scaled -= term;
        }

        index++;
    }

    rt_kprintf("pi_load done: pi ~= %d.%06d, terms=%u\n",
               (int)(pi_scaled / PI_SCALE),
               (int)(pi_scaled % PI_SCALE),
               index);
}

static void pi_load(int argc, char **argv)
{
    rt_uint32_t seconds = PI_LOAD_DEFAULT_SECONDS;

    if (argc > 1)
    {
        seconds = (rt_uint32_t)strtoul(argv[1], RT_NULL, 0);
        if (seconds == 0U)
        {
            seconds = PI_LOAD_DEFAULT_SECONDS;
        }
        else if (seconds > PI_LOAD_MAX_SECONDS)
        {
            seconds = PI_LOAD_MAX_SECONDS;
        }
    }

    rt_kprintf("pi_load running: %u seconds\n", seconds);
    pi_load_run(seconds);
}
MSH_CMD_EXPORT(pi_load, run foreground pi calculation load test: pi_load [seconds]);
