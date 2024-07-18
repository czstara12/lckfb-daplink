/*
* 梁山派软硬件资料与相关扩展板软硬件资料官网全部开源
* 开发板官网：www.lckfb.com
* 技术支持常驻论坛，任何技术问题欢迎随时交流学习
* 立创论坛：club.szlcsc.com
* 关注bilibili账号：【立创开发板】，掌握我们的最新动态！
* 不靠卖板赚钱，以培养中国工程师为己任
* Change Logs:
* Date           Author       Notes
* 2024-04-25     LCKFB-yzh    first version
*/
#include <stdio.h>
#include <board.h>
#include "string.h"
#include "math.h"


void floatToString(char *buffer, size_t buffer_size, float value, uint16_t decimals)
{
    if (buffer == NULL || buffer_size == 0)
        return;

    // 确定数值的符号，处理负数
    char sign = (value < 0) ? '-' : '+';
    value = fabs(value);

    // 计算整数部分
    int int_part = (int)value;
    float remainder = value - (float)int_part;

    // 转换整数部分，考虑数值符号
    int offset = 0;
    if (sign == '-')
    {
        offset += rt_snprintf(buffer + offset, buffer_size - offset, "-%d", int_part);
    }
    else
    {
        offset += rt_snprintf(buffer + offset, buffer_size - offset, "%d", int_part);
    }

    // 如果需要小数点后的数字
    if (decimals > 0)
    {
        // 在缓冲区中追加小数点
        if (offset < buffer_size - 1)
        {
            buffer[offset++] = '.';
        }

        // 转换小数部分
        for (int i = 0; i < decimals; ++i)
        {
            if (offset < buffer_size - 1)
            {
                remainder *= 10;
                int digit = (int)remainder;
                remainder -= (float)digit;
                buffer[offset++] = '0' + digit; // 直接将数字追加到字符串
            }
        }

        // 确保字符串正确结束
        if (offset < buffer_size)
        {
            buffer[offset] = '\0';
        }
    }
}
