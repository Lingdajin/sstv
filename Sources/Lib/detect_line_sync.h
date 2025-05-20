#ifndef DETECT_LINE_SYNC_H
#define DETECT_LINE_SYNC_H

#include "user_task.h"


// 定义常量
#define SAMPLE_RATE 48000       // 原始采样率
#define FREQ_1200 1200          // 第一个目标频率(Hz)
#define FREQ_1500 1500          // 第二个目标频率(Hz)
#define FREQ_TOLERANCE 50       // 频率容差(Hz)
#define WINDOW_1MS 48           // 1ms窗口大小 (48000 * 0.001)
#define STEP_1MS 48             // 1ms步长

// 状态机状态
typedef enum {
    STATE_IDLE,                // 初始状态，搜索第一个1200Hz
    STATE_FOUND_1200_FIRST,    // 找到第一个1200Hz，准备跳跃
    STATE_SEARCHING_1500,      // 搜索1500Hz信号
    STATE_COMPLETE             // 完整检测到序列
} Line_State;


// 函数声明

// 计算频率均值
extern short calc_freq_avg(short* freq_data, int length);

// 检查频率是否在目标频率容差范围内
extern int is_freq_match(short freq, short target_freq);

vis_result detect_line_sync(FILE* file_i, FILE* file_q, int search_start, int search_length);
vis_result detect_line_sync_use(FILE* file_i, FILE* file_q, int search_start, int search_length);

#endif // DETECT_LINE_SYNC_H
