/*
 * 立创开发板软硬件资料与相关扩展板软硬件资料官网全部开源
 * 开发板官网：www.lckfb.com
 * 技术支持常驻论坛，任何技术问题欢迎随时交流学习
 * 立创论坛：club.szlcsc.com
 * 关注bilibili账号：【立创开发板】，掌握我们的最新动态！
 * 不靠卖板赚钱，以培养中国工程师为己任
 * Change Logs:
 * Date           Author       Notes
 * 2024-04-11     LCKFB-yzh    first version
 */

#include "ui.h"
#include "DAP.h"
#include "lvgl_data_update.h"
#include "swd_download_file.h"
#include "uMCN.h"
#include "adc_convert.h"
#include "numeric_tools.h"
#include "rtdevice.h"

#define LOG_TAG     "lvgl_data_update"     // 该模块对应的标签。不定义时，默认：NO_TAG
#define LOG_LVL     LOG_LVL_DBG   // 该模块对应的日志输出级别。不定义时，默认：调试级别
#include <ulog.h>                 // 必须在 LOG_TAG 与 LOG_LVL 下面

char static _lvgl_temp_char[100];
static char _lvgl_daplink_uart_is_need_update = 0;


static McnNode_t adc_filter_data_node;
MCN_DECLARE(adc_filter_data_topic);

static McnNode_t adc_filter_data_node2;



void set_daplink_uart_is_need_update(void)
{
    _lvgl_daplink_uart_is_need_update = 1;
}
void reset_daplink_uart_is_need_update(void)
{
    _lvgl_daplink_uart_is_need_update = 0;
}

void update_daplink_uart_data(void)
{
    char _uart_parity_type[5] = {0};
    char _uart_stop_bit[5] = {0};
    if(_lvgl_daplink_uart_is_need_update == 1)
    {

        if (get_cdc_g_line_coding_bParityType() == 1)
        {
            rt_sprintf(_uart_parity_type, "%s", "ODD");
        }
        else if (get_cdc_g_line_coding_bParityType() == 2)
        {
            rt_sprintf(_uart_parity_type, "%s", "EVEN");
        }
        else
        {
            rt_sprintf(_uart_parity_type, "%s", "NONE");
        }

        if (get_cdc_g_line_coding_bCharFormat() == 1)
        {
            rt_sprintf(_uart_stop_bit, "%s", "1.5");
        }
        else if (get_cdc_g_line_coding_bCharFormat() == 2)
        {
            rt_sprintf(_uart_stop_bit, "%s", "2");
        }
        else
        {
            rt_sprintf(_uart_stop_bit, "%s", "1");
        }
        if(get_cdc_g_line_coding_dwDTERate() == 0)
        {
            rt_sprintf(_lvgl_temp_char, "%s", "UART:NOT OPEN");
        }else
        {
            rt_sprintf(_lvgl_temp_char, "%d,%d,%s,%s", get_cdc_g_line_coding_dwDTERate(),get_cdc_g_line_coding_bDataBits(),_uart_parity_type,_uart_stop_bit);
        }
        lv_label_set_text(ui_uartInfoL, _lvgl_temp_char);

    }
    reset_daplink_uart_is_need_update();
}


extern char volatile current_dap_mode;
extern          DAP_Data_t DAP_Data;            // DAP Data

//     CLOCK  delay
//     5Khz  5600
//     10Khz 2800
//     20Khz 1400
//     50Khz 560
//     100Khz 280
//     200Khz 140
//     500Khz 56
//     1Mhz 28
//     2Mhz 14
//     5Mhz 5
//     10Mhz 3
void update_daplink_dbg_data(void)
{
    static uint8_t _last_dap_mode = 0;
    static uint32_t _last_clock_delay = 0;

    if (_last_dap_mode != current_dap_mode)
    {
        if (current_dap_mode == 1)
        {
            lv_label_set_text(ui_dbgModeL, "SWD");
        }
        else if (current_dap_mode == 2)
        {
            lv_label_set_text(ui_dbgModeL, "JTAG");
        }
    }
    if (_last_clock_delay != DAP_Data.clock_delay)
    {
        if (DAP_Data.clock_delay == 3)
        {
            lv_label_set_text(ui_dbgClockL, "10M Hz");
        }
        else if (DAP_Data.clock_delay == 5)
        {
            lv_label_set_text(ui_dbgClockL, "5M Hz");
        }
        else if (DAP_Data.clock_delay == 14)
        {
            lv_label_set_text(ui_dbgClockL, "2M Hz");
        }
        else if (DAP_Data.clock_delay == 28)
        {
            lv_label_set_text(ui_dbgClockL, "1M Hz");
        }
        else if (DAP_Data.clock_delay == 56)
        {
            lv_label_set_text(ui_dbgClockL, "500K Hz");
        }
        else if (DAP_Data.clock_delay == 140)
        {
            lv_label_set_text(ui_dbgClockL, "200K Hz");
        }
        else if (DAP_Data.clock_delay == 280)
        {
            lv_label_set_text(ui_dbgClockL, "100K Hz");
        }
        else if (DAP_Data.clock_delay == 560)
        {
            lv_label_set_text(ui_dbgClockL, "50K Hz");
        }
        else if (DAP_Data.clock_delay == 1400)
        {
            lv_label_set_text(ui_dbgClockL, "20K Hz");
        }
        else if (DAP_Data.clock_delay == 2800)
        {
            lv_label_set_text(ui_dbgClockL, "10K Hz");
        }
        else if (DAP_Data.clock_delay == 5600)
        {
            lv_label_set_text(ui_dbgClockL, "5K Hz");
        }
    }
    _last_dap_mode = current_dap_mode;
    _last_clock_delay = DAP_Data.clock_delay;
}

extern struct offline_download_info_t offline_download_info;

void update_offline_downlaod_info(void)
{
    static struct offline_download_info_t last_offline_download_info;
    char _temp_char[10] = {0};

    if (last_offline_download_info.success_download_count
        != offline_download_info.success_download_count)
    {
        rt_snprintf(_temp_char,
                    sizeof(_temp_char), "%d",
                    offline_download_info.success_download_count);
        lv_label_set_text(ui_ODdownloadSuccessCount, _temp_char);
    }
    if(last_offline_download_info.progress!=offline_download_info.progress)
    {
        lv_bar_set_value(ui_OfflineDownloadProcessBar, offline_download_info.progress, LV_ANIM_ON);
        rt_snprintf(_temp_char,
                    sizeof(_temp_char), "%d%",
                    offline_download_info.progress);
        lv_label_set_text(ui_OfflineDownloadProcessNum, _temp_char);
    }

    if(rt_strncmp(last_offline_download_info.info_message, offline_download_info.info_message,sizeof(offline_download_info.info_message))!=0)
    {
        lv_label_set_text(ui_ODdownloadInfo, offline_download_info.info_message);
    }
    rt_memcpy(&last_offline_download_info, &offline_download_info, sizeof(last_offline_download_info));

}

void update_daplink_voltage_current_data(void)
{
    static char _V_5V0_text_buf[10];
    static char _I_5V0_text_buf[10];
    static char _V_3V3_text_buf[10];
    static char _I_3V3_text_buf[10];
    static char _V_PD_text_buf[10];
    static char _I_PD_text_buf[10];

    static float _i_pd_measure = 0;
    static float last_i_pd_measure = 0;

    static adc_filter_data_t read_data;
    static adc_filter_data_t last_read_data;

    if (mcn_poll(adc_filter_data_node))
    {
        mcn_copy(MCN_HUB(adc_filter_data_topic), adc_filter_data_node,
                 &read_data);
    }

    if(GPIOB_INPUT(11) == 0)
    {
        _i_pd_measure = read_data.i_pd_measure2;
    }else
    {
        _i_pd_measure = read_data.i_pd_measure1;
    }

    floatToString(_V_5V0_text_buf, sizeof(_V_5V0_text_buf),read_data.v_5v0_measure, 2);
    if(read_data.i_5v0_measure<100)
    {
        floatToString(_I_5V0_text_buf, sizeof(_I_5V0_text_buf),
                      read_data.i_5v0_measure, 2);
    }
    else
    {
        floatToString(_I_5V0_text_buf, sizeof(_I_5V0_text_buf),
                      read_data.i_5v0_measure, 1);
    }
    floatToString(_V_3V3_text_buf, sizeof(_V_3V3_text_buf),read_data.v_3v3_measure, 2);
    if (read_data.i_3v3_measure < 100)
    {
        floatToString(_I_3V3_text_buf, sizeof(_I_3V3_text_buf),
                      read_data.i_3v3_measure, 2);
    }
    else
    {
        floatToString(_I_3V3_text_buf, sizeof(_I_3V3_text_buf),
                      read_data.i_3v3_measure, 1);
    }

    floatToString(_V_PD_text_buf, sizeof(_V_PD_text_buf),read_data.v_pd_measure, 2);
    if (_i_pd_measure < 100)
    {
        floatToString(_I_PD_text_buf, sizeof(_I_PD_text_buf),_i_pd_measure, 2);
    }
    else
    {
        floatToString(_I_PD_text_buf, sizeof(_I_PD_text_buf),_i_pd_measure, 1);
    }

    rt_snprintf(_V_5V0_text_buf, sizeof(_V_5V0_text_buf),"%s%s",_V_5V0_text_buf,"V");
    rt_snprintf(_I_5V0_text_buf, sizeof(_I_5V0_text_buf),"%s%s",_I_5V0_text_buf,"mA");
    rt_snprintf(_V_3V3_text_buf, sizeof(_V_3V3_text_buf),"%s%s",_V_3V3_text_buf,"V");
    rt_snprintf(_I_3V3_text_buf, sizeof(_I_3V3_text_buf),"%s%s",_I_3V3_text_buf,"mA");
    rt_snprintf(_V_PD_text_buf, sizeof(_V_PD_text_buf),"%s%s",_V_PD_text_buf,"V");
    rt_snprintf(_I_PD_text_buf, sizeof(_I_PD_text_buf),"%s%s",_I_PD_text_buf,"mA");

    if (last_read_data.v_5v0_measure != read_data.v_5v0_measure)
        lv_label_set_text(ui_POWER5VMeasure, _V_5V0_text_buf);
    if (last_read_data.i_5v0_measure != read_data.i_5v0_measure)
        lv_label_set_text(ui_POWER5CMeasure, _I_5V0_text_buf);
    if (last_read_data.v_3v3_measure != read_data.v_3v3_measure)
        lv_label_set_text(ui_POWER3VMeasure, _V_3V3_text_buf);
    if (last_read_data.i_3v3_measure != read_data.i_3v3_measure)
        lv_label_set_text(ui_POWER3CMeasure, _I_3V3_text_buf);
    if (last_read_data.v_pd_measure != read_data.v_pd_measure)
        lv_label_set_text(ui_POWERPDVmeasure, _V_PD_text_buf);
    if (last_i_pd_measure != _i_pd_measure)
        lv_label_set_text(ui_POWERPDImeasure, _I_PD_text_buf);

    last_read_data.i_5v0_measure = read_data.i_5v0_measure;
    last_read_data.v_5v0_measure = read_data.v_5v0_measure;
    last_read_data.i_3v3_measure = read_data.i_3v3_measure;
    last_read_data.v_3v3_measure = read_data.v_3v3_measure;
    last_i_pd_measure = _i_pd_measure;
    last_read_data.v_pd_measure = read_data.v_pd_measure;
	
}

void update_volt_ammeter_voltage_current_data(void)
{
    static char _V_5V0_text_buf[10];
    static char _I_5V0_text_buf[10];
    static char _P_5V0_text_buf[10];
    static char _V_3V3_text_buf[10];
    static char _I_3V3_text_buf[10];
    static char _P_3V3_text_buf[10];
    static char _V_PD_text_buf[10];
    static char _I_PD_text_buf[10];
    static char _P_PD_text_buf[10];

    static adc_filter_data_t read_data;
    static adc_filter_data_t last_read_data;

    static float _i_pd_measure;
    static float last_i_pd_measure;

    static float power_5V0_mW;
    static float power_3V3_mW;
    static float power_PD_mW;

    static float last_power_5V0_mW;
    static float last_power_3V3_mW;
    static float last_power_PD_mW;


    if (mcn_poll(adc_filter_data_node2))
    {
        mcn_copy(MCN_HUB(adc_filter_data_topic), adc_filter_data_node2,
                 &read_data);
    }

    if(GPIOB_INPUT(11) == 0)
    {
        _i_pd_measure = read_data.i_pd_measure2;
    }else
    {
        _i_pd_measure = read_data.i_pd_measure1;
    }

    power_5V0_mW = read_data.v_5v0_measure * read_data.i_5v0_measure;
    power_3V3_mW = read_data.v_3v3_measure * read_data.i_3v3_measure;
    power_PD_mW = read_data.v_pd_measure * _i_pd_measure;

    floatToString(_V_5V0_text_buf, sizeof(_V_5V0_text_buf),read_data.v_5v0_measure, 3);
    floatToString(_I_5V0_text_buf, sizeof(_I_5V0_text_buf),read_data.i_5v0_measure, 2);
    floatToString(_V_3V3_text_buf, sizeof(_V_3V3_text_buf),read_data.v_3v3_measure, 3);
    floatToString(_I_3V3_text_buf, sizeof(_I_3V3_text_buf),read_data.i_3v3_measure, 2);
    floatToString(_V_PD_text_buf, sizeof(_V_PD_text_buf),read_data.v_pd_measure, 2);
    floatToString(_I_PD_text_buf, sizeof(_I_PD_text_buf),_i_pd_measure, 2);

    rt_snprintf(_V_5V0_text_buf, sizeof(_V_5V0_text_buf), "%s%s",
                _V_5V0_text_buf, "  V");
    rt_snprintf(_I_5V0_text_buf, sizeof(_I_5V0_text_buf), "%s%s",
                _I_5V0_text_buf, " mA");
    rt_snprintf(_V_3V3_text_buf, sizeof(_V_3V3_text_buf), "%s%s",
                _V_3V3_text_buf, "  V");
    rt_snprintf(_I_3V3_text_buf, sizeof(_I_3V3_text_buf), "%s%s",
                _I_3V3_text_buf, " mA");
    rt_snprintf(_V_PD_text_buf, sizeof(_V_PD_text_buf), "%s%s", _V_PD_text_buf,
                "  V");
    rt_snprintf(_I_PD_text_buf, sizeof(_I_PD_text_buf), "%s%s", _I_PD_text_buf,
                " mA");

    floatToString(_P_5V0_text_buf, sizeof(_P_5V0_text_buf),power_5V0_mW, 2);
    rt_snprintf(_P_5V0_text_buf, sizeof(_P_5V0_text_buf), "%s%s",
                _P_5V0_text_buf, " mW");
    floatToString(_P_3V3_text_buf, sizeof(_P_3V3_text_buf),power_3V3_mW, 2);
    rt_snprintf(_P_3V3_text_buf, sizeof(_P_3V3_text_buf), "%s%s",
                _P_3V3_text_buf, " mW");
    if(power_PD_mW<1000)
    {
        floatToString(_P_PD_text_buf, sizeof(_P_PD_text_buf), power_PD_mW, 2);
        rt_snprintf(_P_PD_text_buf, sizeof(_P_PD_text_buf), "%s%s",
                    _P_PD_text_buf, " mW");
    }else
    {
        floatToString(_P_PD_text_buf, sizeof(_P_PD_text_buf), power_PD_mW / 1000, 2);
        rt_snprintf(_P_PD_text_buf, sizeof(_P_PD_text_buf), "%s%s",
                    _P_PD_text_buf, "  W");
    }

    if (last_read_data.v_5v0_measure != read_data.v_5v0_measure)
        lv_label_set_text(ui_MPOWER5VMeasureL, _V_5V0_text_buf);
    if (last_read_data.i_5v0_measure != read_data.i_5v0_measure)
        lv_label_set_text(ui_MPOWER5IMeasureL, _I_5V0_text_buf);
    if (last_read_data.v_3v3_measure != read_data.v_3v3_measure)
        lv_label_set_text(ui_MPOWER3VMeasureL, _V_3V3_text_buf);
    if (last_read_data.i_3v3_measure != read_data.i_3v3_measure)
        lv_label_set_text(ui_MPOWER3IMeasureL, _I_3V3_text_buf);
    if (last_read_data.v_pd_measure != read_data.v_pd_measure)
        lv_label_set_text(ui_MPOWERPDVMeasureL, _V_PD_text_buf);
    if (last_i_pd_measure != _i_pd_measure)
        lv_label_set_text(ui_MPOWERPDIMeasureL, _I_PD_text_buf);
    if(last_power_5V0_mW != power_5V0_mW)
        lv_label_set_text(ui_MPOWER5PMeasureL, _P_5V0_text_buf);
    if(last_power_3V3_mW != power_3V3_mW)
        lv_label_set_text(ui_MPOWER3PMeasureL, _P_3V3_text_buf);
    if(last_power_PD_mW != power_PD_mW)
        lv_label_set_text(ui_MPOWERPDPMeasureL, _P_PD_text_buf);

    last_read_data.i_5v0_measure = read_data.i_5v0_measure;
    last_read_data.v_5v0_measure = read_data.v_5v0_measure;
    last_read_data.i_3v3_measure = read_data.i_3v3_measure;
    last_read_data.v_3v3_measure = read_data.v_3v3_measure;
    last_i_pd_measure = _i_pd_measure;
    last_read_data.v_pd_measure = read_data.v_pd_measure;
    last_power_5V0_mW = power_5V0_mW;
    last_power_3V3_mW = power_3V3_mW;
    last_power_PD_mW = power_PD_mW;
}

#include "power_control.h"
void update_volt_ammeter_pd_sink_state(void)
{
	if(rt_pin_read(PIN_PD_SINK_STATE) == PIN_HIGH)
	{
		lv_obj_add_state(ui_PDSinkState,LV_STATE_CHECKED);
	}
	else
	{
		lv_obj_clear_state(ui_PDSinkState,LV_STATE_CHECKED);
	}
}

extern uint32_t get_idcode(void);
void update_daplink_idcode(void)
{
    static char _temp_char[40] = {0};
    static uint32_t last_idcode = 0;
    static uint32_t current_idcode = 0;

    current_idcode = get_idcode();
    if (last_idcode != current_idcode)
    {
        if(current_idcode == 0)
        {
            rt_snprintf(_temp_char, sizeof(_temp_char), "IDCODE:  NONE");
            lv_label_set_text(ui_dbgIDCODEL, _temp_char);
        }
		else
		{
        rt_snprintf(_temp_char, sizeof(_temp_char), "IDCODE:%lX", current_idcode);
        lv_label_set_text(ui_dbgIDCODEL, _temp_char);
		}
    }

    last_idcode = current_idcode;
}

void check_and_clear_textarea(lv_obj_t *ta, uint32_t max_length)
{
	static uint32_t current_length;
    // 假设你已经有了一个指向text area对象的指针lv_obj_t * ta
    const char * text = lv_textarea_get_text(ta);

    current_length = rt_strlen(text);
    if (current_length > max_length) { // 检查文本长度是否超过最大长度
        lv_textarea_set_text(ta, ""); // 清除文本区域的内容
    }
}


void update_uart_monitor_text_area(void)
{
    uint32_t usart_rx_size = 0;
    uint8_t* buffer = RT_NULL;

    usart_rx_size = chry_ringbuffer_get_used(&g_uartrx_for_lvgl);
    if (usart_rx_size) {

        buffer = rt_malloc(usart_rx_size + 1);  // 动态分配内存，并额外分配一个字节用于null字符
        if (buffer == RT_NULL)  // 使用双等号进行比较
        {
            LOG_E("usart_rx_size malloc error!");
            return;
        }

        chry_ringbuffer_read(&g_uartrx_for_lvgl, buffer, usart_rx_size);
        buffer[usart_rx_size] = '\0';  // 确保字符串以null结尾
        lv_textarea_add_text(ui_UMTextArea, (char*)buffer);

        rt_free(buffer);
    }
	check_and_clear_textarea(ui_UMTextArea,CONFIG_UARTRX_RINGBUF_SIZE_FOR_LVGL);
}

extern struct rt_device_pwm *pwm_dev; /* PWM设备句柄 */
void update_pwm_frequency_and_duty(void)
{
    char _temp_char[20] = {0};
    static struct rt_pwm_configuration last_pwm_config;
    static struct rt_pwm_configuration current_pwm_config;

    static float pwm_output_frequency = 0;
    static float pwm_output_duty = 0;

	current_pwm_config.channel = 1;
	
    if (pwm_dev == RT_NULL)
    {
        rt_kprintf("get pwm device run failed! can't find %s device!!\n", "pwm1");
        return;
    }

    rt_err_t rt_pwm_get(struct rt_device_pwm *device, struct rt_pwm_configuration *cfg);
    rt_pwm_get(pwm_dev, &current_pwm_config);

    if (last_pwm_config.period != current_pwm_config.period)
    {
        pwm_output_frequency = 1.0f/(double)(current_pwm_config.period/(double)1000000000.0f);

        if(pwm_output_frequency < 1000.0f)
        {

            floatToString(_temp_char, sizeof(_temp_char), pwm_output_frequency,
                          2);

            rt_snprintf(_temp_char, sizeof(_temp_char), "%sHz", _temp_char);
        }else if (pwm_output_frequency < 1000000.0f)
        {
            pwm_output_frequency = pwm_output_frequency/1000.0f;
            floatToString(_temp_char, sizeof(_temp_char), pwm_output_frequency,
                          1);

            rt_snprintf(_temp_char, sizeof(_temp_char), "%sKHz", _temp_char);
        }else if (pwm_output_frequency < 1000000000.0f)
        {
            pwm_output_frequency = pwm_output_frequency/1000000.0f;
            floatToString(_temp_char, sizeof(_temp_char), pwm_output_frequency,
                          3);

            rt_snprintf(_temp_char, sizeof(_temp_char), "%sMHz", _temp_char);
        }

        lv_label_set_text(ui_PWMfrequencyLabel, _temp_char);
    }
    if (last_pwm_config.pulse != current_pwm_config.pulse)
    {
        pwm_output_duty=((double)current_pwm_config.pulse/(double)current_pwm_config.period)*100.0f;

        floatToString(_temp_char,sizeof(_temp_char),pwm_output_duty,3);

        rt_snprintf(_temp_char, sizeof(_temp_char), "%s%%", _temp_char);

        lv_label_set_text(ui_PWMDutyCyclelabel, _temp_char);
    }
    last_pwm_config.pulse=current_pwm_config.pulse;
    last_pwm_config.period=current_pwm_config.period;

}


int lvgl_data_update_prev_init(void)
{

    adc_filter_data_node = mcn_subscribe(MCN_HUB(adc_filter_data_topic), RT_NULL, RT_NULL);
    adc_filter_data_node2 = mcn_subscribe(MCN_HUB(adc_filter_data_topic), RT_NULL, RT_NULL);

    return 1;
}

INIT_PREV_EXPORT(lvgl_data_update_prev_init);
