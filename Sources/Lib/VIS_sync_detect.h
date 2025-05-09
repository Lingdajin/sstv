#include <stdio.h>
#include <stdlib.h>
#include <math.h>

// VIS同步头参数定义
#define VIS_SYNC_HIGH_FREQ       1900  // 同步高频率(Hz)
#define VIS_SYNC_LOW_FREQ        1200  // 同步低频率(Hz)
#define VIS_SYNC_FIRST_DURATION   300  // 第一段1900Hz持续时间(ms)
#define VIS_SYNC_BREAK_DURATION    10  // 1200Hz中断持续时间(ms)
#define VIS_SYNC_SECOND_DURATION  300  // 第二段1900Hz持续时间(ms)
#define VIS_CORR_THRESHOLD        0.6  // 相关性阈值

// VIS同步头检测结果结构
typedef struct {
    int found;           // 是否找到同步头(1=找到, 0=未找到)
    int start_position;  // 同步头开始位置
    int data_position;   // VIS数据开始位置(同步头结束后的位置)
    float confidence;    // 检测置信度(0-1)
} vis_sync_result;

// 函数声明
vis_sync_result detect_vis_header(FILE* file_i, FILE* file_q, int search_start, int search_length);