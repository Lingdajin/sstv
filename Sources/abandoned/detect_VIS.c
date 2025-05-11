#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <math.h>
#include <string.h>

// 定义常量
#define M_PI 3.14159265358979323846
#define SAMPLE_RATE 48000       // 原始采样率
#define DOWN_SAMPLE_RATE 8000   // 下采样后的采样率
#define DOWN_FACTOR 6           // 下采样因子 (48000 / 8000)
#define FRAME_SIZE 80           // 下采样后10ms对应的样本数
#define SCALE_SHIFT 15          // 定点数缩放位数
#define ENERGY_THRESHOLD 3000 // 能量阈值，根据实际信号调整

// 状态机状态定义
typedef enum {
    DETECT_IDLE,         // 空闲状态，等待首个1900Hz信号
    DETECT_1900_FIRST,   // 检测到第一个1900Hz信号
    DETECT_1200,         // 检测到1200Hz信号
    DETECT_1900_SECOND,  // 检测到第二个1900Hz信号
    DETECT_COMPLETE      // 完成检测序列
} DetectState;

// 检测结果结构体
typedef struct {
    int detected;           // 是否检测到完整序列
    int start_position;     // 序列开始位置
    int end_position;       // 序列结束位置
} DetectResult;

// 生成正弦余弦查找表
void generate_sin_cos_table(unsigned char *cos_tab, unsigned char *sin_tab, float freq) {
    for (int i = 0; i < FRAME_SIZE; ++i) {
        float phase = 2 * M_PI * freq * i / DOWN_SAMPLE_RATE;
        cos_tab[i] = (unsigned char)(cosf(phase) * (1 << SCALE_SHIFT)); // 固定点
        sin_tab[i] = (unsigned char)(sinf(phase) * (1 << SCALE_SHIFT));
    }
}

// 检测特定频率能量
int detect_freq_energy(const unsigned char *I, const unsigned char *Q,
                           const unsigned char *cos_tab, const unsigned char *sin_tab) {
    int acc_I = 0;
    int acc_Q = 0;

    for (int i = 0; i < FRAME_SIZE; ++i) {
        acc_I += (I[i] * cos_tab[i] - Q[i] * sin_tab[i]) >> SCALE_SHIFT;
        acc_Q += (I[i] * sin_tab[i] + Q[i] * cos_tab[i]) >> SCALE_SHIFT;
    }

    return acc_I * acc_I + acc_Q * acc_Q;
}

// 下采样函数
void downsample(const signed char *input, unsigned char *output, int input_length, int down_factor) {
    for (int i = 0; i < input_length / down_factor; i++) {
        output[i] = (unsigned char)input[i * down_factor];
    }
}

// 检测VIS码序列
DetectResult detect_VIS_sequence(FILE *file_i, FILE *file_q, int search_start, int search_length) {
    DetectResult result = {0, -1, -1};
    
    // 预先生成1900Hz和1200Hz的正余弦表
    unsigned char cos_1900[FRAME_SIZE], sin_1900[FRAME_SIZE];
    unsigned char cos_1200[FRAME_SIZE], sin_1200[FRAME_SIZE];
    
    generate_sin_cos_table(cos_1900, sin_1900, 1900.0f);
    generate_sin_cos_table(cos_1200, sin_1200, 1200.0f);
    
    // 分配输入缓冲区
    int frame_read_size = FRAME_SIZE * DOWN_FACTOR; // 原始采样率下的帧大小
    signed char *i_buffer_raw = (signed char *)malloc(frame_read_size);
    signed char *q_buffer_raw = (signed char *)malloc(frame_read_size);
    unsigned char *I = (unsigned char *)malloc(FRAME_SIZE * sizeof(unsigned char));
    unsigned char *Q = (unsigned char *)malloc(FRAME_SIZE * sizeof(unsigned char));
    
    if (!i_buffer_raw || !q_buffer_raw || !I || !Q) {
        printf("内存分配失败\n");
        if (i_buffer_raw) free(i_buffer_raw);
        if (q_buffer_raw) free(q_buffer_raw);
        if (I) free(I);
        if (Q) free(Q);
        return result;
    }
    
    // 状态机变量
    DetectState state = DETECT_IDLE;
    int frame_count = 0;
    int position = search_start;
    int sequence_start = -1;
    
    printf("开始检测VIS码序列...\n");
    
    // 帧级状态机处理
    while (position < search_start + search_length - frame_read_size) {
        // 读取一帧数据
        if (fseek(file_i, position, SEEK_SET) != 0 ||
            fseek(file_q, position, SEEK_SET) != 0) {
            printf("文件定位失败\n");
            break;
        }
        
        size_t read_i = fread(i_buffer_raw, 1, frame_read_size, file_i);
        size_t read_q = fread(q_buffer_raw, 1, frame_read_size, file_q);
        
        if (read_i != frame_read_size || read_q != frame_read_size) {
            printf("读取数据失败\n");
            break;
        }
        
        // 下采样
        downsample(i_buffer_raw, I, frame_read_size, DOWN_FACTOR);
        downsample(q_buffer_raw, Q, frame_read_size, DOWN_FACTOR);
        
        // 计算1900Hz和1200Hz的能量
        int energy_1900 = detect_freq_energy(I, Q, cos_1900, sin_1900);
        printf("1900hz: %d\r\n", energy_1900);
        int energy_1200 = detect_freq_energy(I, Q, cos_1200, sin_1200);
        printf("1200hz: %d\r\n", energy_1200);

        // 状态机处理
        switch (state) {
            case DETECT_IDLE:
                if (energy_1900 > ENERGY_THRESHOLD && energy_1900 > energy_1200 * 2) {
                    // 检测到第一个1900Hz信号
                    state = DETECT_1900_FIRST;
                    sequence_start = position;
                    frame_count = 1;
                    printf("检测到第一个1900Hz信号: 位置=%d, 能量=%d\n", 
                          position, energy_1900);
                }
                break;
                
            case DETECT_1900_FIRST:
                frame_count++;
                // 约10ms后应该是1200Hz信号
                if (frame_count >= 1) {  // 一帧正好是10ms
                    if (energy_1200 > ENERGY_THRESHOLD && energy_1200 > energy_1900 * 1.5) {
                        // 检测到1200Hz信号
                        state = DETECT_1200;
                        frame_count = 1;
                        printf("检测到1200Hz信号: 位置=%d, 能量=%d\n", 
                              position, energy_1200);
                    } else if (frame_count > 3) {  // 允许一些容错
                        // 超过容错范围，重置状态
                        state = DETECT_IDLE;
                        printf("未检测到预期的1200Hz信号，重置\n");
                    }
                }
                break;
                
            case DETECT_1200:
                frame_count++;
                // 约10ms后应该是第二个1900Hz信号
                if (frame_count >= 1) {  // 一帧正好是10ms
                    if (energy_1900 > ENERGY_THRESHOLD && energy_1900 > energy_1200 * 1.5) {
                        // 检测到第二个1900Hz信号，序列完成
                        state = DETECT_COMPLETE;
                        printf("检测到第二个1900Hz信号: 位置=%d, 能量=%d\n", 
                              position, energy_1900);
                        
                        // 设置检测结果
                        result.detected = 1;
                        result.start_position = sequence_start;
                        result.end_position = position + frame_read_size;
                        
                        printf("成功检测到完整VIS码序列!\n");
                        printf("序列起始: %d, 结束: %d, 持续时间: %.2fms\n",
                              result.start_position, result.end_position,
                              (result.end_position - result.start_position) * 1000.0 / SAMPLE_RATE);
                        
                        // 找到序列后退出循环
                        goto cleanup;
                    } else if (frame_count > 3) {  // 允许一些容错
                        // 超过容错范围，重置状态
                        state = DETECT_IDLE;
                        printf("未检测到预期的第二个1900Hz信号，重置\n");
                    }
                }
                break;
                
            case DETECT_COMPLETE:
                // 此状态不应在循环中出现，因为找到序列后应立即退出
                break;
        }
        
        // 移动到下一帧
        position += frame_read_size / 2;  // 50%重叠
    }
    
cleanup:
    // 释放内存
    free(i_buffer_raw);
    free(q_buffer_raw);
    free(I);
    free(Q);
    
    return result;
}

// 主函数
// int main() {
//     const char* iq_data_i_path = "./sstv_data_file/sstv_iq_i_scaled_big_endian.bin";
//     const char* iq_data_q_path = "./sstv_data_file/sstv_iq_q_scaled_big_endian.bin";
    
//     FILE* file_i = fopen(iq_data_i_path, "rb");
//     FILE* file_q = fopen(iq_data_q_path, "rb");
    
//     if (file_i == NULL || file_q == NULL) {
//         printf("无法打开IQ数据文件\n");
//         if (file_i) fclose(file_i);
//         if (file_q) fclose(file_q);
//         return 1;
//     }
    
//     // 设置搜索范围
//     int search_start = 0;
//     int search_length = SAMPLE_RATE * 0.3;  // 搜索前10秒数据
    
//     DetectResult result = detect_VIS_sequence(file_i, file_q, search_start, search_length);
    
//     if (result.detected) {
//         printf("\nVIS码序列检测结果:\n");
//         printf("- 起始位置: %d (%.3f秒)\n", 
//                result.start_position, 
//                result.start_position / (float)SAMPLE_RATE);
//         printf("- 结束位置: %d (%.3f秒)\n", 
//                result.end_position, 
//                result.end_position / (float)SAMPLE_RATE);
//         printf("- 持续时间: %.2f毫秒\n", 
//                (result.end_position - result.start_position) * 1000.0 / SAMPLE_RATE);
//     } else {
//         printf("\n未检测到完整VIS码序列\n");
//     }
    
//     fclose(file_i);
//     fclose(file_q);
//     return 0;
// }
