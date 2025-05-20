#ifndef DETECT_VIS_SYNC_H
#define DETECT_VIS_SYNC_H

#include "user_task.h"


// 定义常量
#define SAMPLE_RATE 48000       // 原始采样率
#define FREQ_1900 1900          // 第一个目标频率(Hz)
#define FREQ_1200 1200          // 第二个目标频率(Hz)
#define FREQ_TOLERANCE 150      // 频率容差(Hz)
#define WINDOW_10MS 480         // 10ms窗口大小 (48000 * 0.01)
#define WINDOW_1MS 48           // 1ms窗口大小 (48000 * 0.001)
#define STEP_10MS 480           // 10ms步长
#define STEP_1MS 48             // 1ms步长
#define JUMP_250MS 12000        // 250ms跳跃(样本数)

// 状态机状态
typedef enum {
    VIS_STATE_IDLE,                // 初始状态，搜索第一个1900Hz
    VIS_STATE_FOUND_1900_FIRST,    // 找到第一个1900Hz，准备跳跃
    VIS_STATE_SEARCHING_1200,      // 搜索1200Hz信号
    VIS_STATE_FOUND_1200,          // 找到1200Hz，搜索第二个1900Hz
    VIS_STATE_COMPLETE             // 完整检测到序列
} VIS_State;



// 函数声明
short calc_freq_avg(short* freq_data, int length);
int is_freq_match(short freq, short target_freq);
vis_result detect_vis_sync(FILE* file_i, FILE* file_q, int search_start, int search_length);
vis_result detect_vis_sync_use(FILE* file_i, FILE* file_q, int search_start, int search_length);

#endif // DETECT_VIS_SYNC_H
