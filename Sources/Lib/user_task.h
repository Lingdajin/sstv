#ifndef USER_TASK_H
#define USER_TASK_H

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "sbdsp/sbatan.h"

// 定点数定义
#define FX_SCALE 1024        // 使用Q10格式 (2^10)
#define FX_SCALE_LOG2 10     // 2^10 = 1024
#define FX_PI 3217           // PI * FX_SCALE
#define FX_DOUBLE(x) ((int)((x) * FX_SCALE + 0.5))  // 浮点数转定点数
#define TO_DOUBLE(x) ((double)(x) / FX_SCALE)       // 定点数转浮点数

// sb_fxatan常量定义
#define SB_FXATAN_MAX 32767   // sb_fxatan输出对应π的最大值
#define SB_FXATAN_PI_SCALE 32767  // sb_fxatan中π对应的比例因子

// 定点数版的常量
#define FX_SYNC_FREQ FX_DOUBLE(1200.0)    // 1200.0 * FX_SCALE
#define FX_BLACK_FREQ FX_DOUBLE(1500.0)   // 1500.0 * FX_SCALE
#define FX_WHITE_FREQ FX_DOUBLE(2300.0)   // 2300.0 * FX_SCALE
#define FX_FREQ_RANGE (FX_WHITE_FREQ - FX_BLACK_FREQ)  // 范围

// 原始浮点常量保留，确保代码兼容性
#define PI 3.14159265358979323846
#define SAMPLE_RATE 48000 // 采样率为48kHz

// SSTV扫描参数
#define Y_NUM 4224        // Y信号采样点数 (88ms * 48kHz)
#define Y_BEGINE 44256    // Y信号开始采样点
#define Y_INTERVAL 7200   // Y信号间隔

#define RY_NUM 2112       // RY信号采样点数 (44ms * 48kHz)
#define RY_BEGINE 48768   // RY信号开始采样点
#define RY_INTERVAL 14400 // RY信号间隔

#define BY_NUM 2112       // BY信号采样点数 (44ms * 48kHz)
#define BY_BEGINE 55968   // BY信号开始采样点
#define BY_INTERVAL 14400 // BY信号间隔

// SSTV频率范围定义
#define SYNC_FREQ 1200
#define BLACK_FREQ 1500
#define WHITE_FREQ 2300
#define FREQ_RANGE (WHITE_FREQ - BLACK_FREQ)

// 图像定义
#define IMAGE_WIDTH 320   // 图像宽度(像素)
#define IMAGE_HEIGHT 240  // 图像高度(行数)
#define TOTAL_SAMPLES 1771680 // 总采样点数

// BMP文件相关定义
#define BYTES_PER_PIXEL 3 // RGB每像素3字节
#define FILE_HEADER_SIZE 14
#define INFO_HEADER_SIZE 40
#define PADDING_SIZE(width) ((4 - ((width) * BYTES_PER_PIXEL) % 4) % 4)

// 结果结构
typedef struct {
    int found;                 // 是否找到同步头
    int start_position;        // 第一个1900Hz开始位置
    int sync_position;         // 1200Hz位置(同步基准点)
    int end_position;          // 第二个1900Hz结束位置
} vis_result;

// 奇偶行检测结果结构
typedef struct {
    int found;                 // 是否成功检测到
    int is_odd;                // 是否为奇数行(1=奇数, 0=偶数)
    int sync_position;         // 门廊1900Hz起始位置 - 关键同步点
    int pulse_frequency;       // 检测到的分割脉冲频率
} parity_result;

// 状态机状态
typedef enum {
    PARITY_STATE_IDLE,                // 初始状态，搜索第一个1200Hz
    PARITY_STATE_FOUND_1500,    // 找到第一个1200Hz，准备跳跃
    PARITY_STATE_FOUND_2300,    // 找到第一个1200Hz，准备跳跃
    PARITY_STATE_SEARCHING_1900,      // 搜索1500Hz信号
    PARITY_STATE_COMPLETE             // 完整检测到序列
} Parity_State;

// 状态机状态
typedef enum {
    VIS_STATE_IDLE,                // 初始状态，搜索第一个1900Hz
    VIS_STATE_FOUND_1900_FIRST,    // 找到第一个1900Hz，准备跳跃
    VIS_STATE_SEARCHING_1200,      // 搜索1200Hz信号
    VIS_STATE_FOUND_1200,          // 找到1200Hz，搜索第二个1900Hz
    VIS_STATE_COMPLETE             // 完整检测到序列
} VIS_State;

// 状态机状态
typedef enum {
    STATE_IDLE,                // 初始状态，搜索第一个1200Hz
    STATE_FOUND_1200_FIRST,    // 找到第一个1200Hz，准备跳跃
    STATE_SEARCHING_1500,      // 搜索1500Hz信号
    STATE_COMPLETE             // 完整检测到序列
} Line_State;

// 函数声明
void fm_demodulate_fx(short* I, short* Q, int length, short* freq_fx, int sample_rate);
void freq_to_yuv_fx(short* freq_fx, int length, short* Y_fx);
void map_to_pixels_fx(short* data_fx, int data_length, short* pixels_fx, int pixel_count);
void yuv_to_rgb(short Y, short RY, short BY, short* R, short* G, short* B);
void save_bmp(const char* filename, unsigned char* image_data, int width, int height);

// 新增函数声明
void extract_scan_line_fx(FILE* file_i, FILE* file_q, int start_sample, int num_samples, short* pixels_fx, int pixel_count);
void process_line_y_fx(FILE* file_i, FILE* file_q, int start_sample, short* y_line_fx);
void process_line_ry_fx(FILE* file_i, FILE* file_q, int ry_start, short* ry_line_fx);
void process_line_by_fx(FILE* file_i, FILE* file_q, int by_start, short* by_line_fx);

// 修改函数声明，用于读取指定范围的IQ数据
int read_iq_data_range_static(FILE* file, int offset, int count, short* buffer);

#endif // USER_TASK_H
