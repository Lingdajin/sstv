#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

// 定义常量
#define SAMPLE_RATE 48000       // 原始采样率
#define FREQ_1200 1200          // 第一个目标频率(Hz)
#define FREQ_1500 1500          // 第二个目标频率(Hz)
#define FREQ_TOLERANCE 150       // 频率容差(Hz)
#define WINDOW_10MS 480         // 10ms窗口大小 (48000 * 0.01)
#define WINDOW_1MS 48           // 1ms窗口大小 (48000 * 0.001)
#define STEP_10MS 480           // 10ms步长
#define STEP_1MS 48             // 1ms步长
#define JUMP_250MS 12000        // 250ms跳跃(样本数)

// 状态机状态
typedef enum {
    STATE_IDLE,                // 初始状态，搜索第一个1900Hz
    STATE_FOUND_1200_FIRST,    // 找到第一个1900Hz，准备跳跃
    STATE_SEARCHING_1500,      // 搜索1200Hz信号
    STATE_COMPLETE             // 完整检测到序列
} VIS_State;

// 结果结构
typedef struct {
    int found;                 // 是否找到line同步头
    int start_position;        // 第一个1200Hz开始位置
    int sync_position;         // 1500Hz位置(同步基准点)
    int end_position;          // 第二个1500Hz结束位置
} vis_result;

// 函数声明
extern int read_iq_data_range_static_BYTE(FILE* file, int offset, int count, signed char* buffer);
extern void fm_demodulate(signed char* I, signed char* Q, int length, short* freq_fx, int sample_rate);
