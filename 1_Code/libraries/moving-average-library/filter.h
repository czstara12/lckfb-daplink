// 本文来自博客园，作者：Sharemaker，转载请注明原文链接：https://www.cnblogs.com/Sharemaker/p/17062418.html

#ifndef _FILTER_H_
#define _FILTER_H_

#include "stdint.h"
#include "rtthread.h"

#define MAX_SENSOR_NUM 7   //使用滤波时的传感器数量
#define MAX_DATA_NUM 20     //最大采样点数量，即采样窗口长度
#define WINDOW_DATA_NUM 10  //滤波窗口长度
//去除采样窗口内最大最小值的数量，这里去除两个最大和两个最小
#define REMOVE_MAXMIN_NUM ((MAX_DATA_NUM - WINDOW_DATA_NUM)/2)

//extern double m_dataList[MAX_SENSOR_NUM][MAX_DATA_NUM];

//声明定义的函数
uint16_t Filter_SlidingWindowAvg(int index, uint16_t data);

#endif
