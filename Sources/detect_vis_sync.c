#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

// 定义常量
#define SAMPLE_RATE 48000       // 原始采样率
#define FREQ_1900 1900          // 第一个目标频率(Hz)
#define FREQ_1200 1200          // 第二个目标频率(Hz)
#define FREQ_TOLERANCE 150       // 频率容差(Hz)
#define WINDOW_10MS 480         // 10ms窗口大小 (48000 * 0.01)
#define WINDOW_1MS 48           // 1ms窗口大小 (48000 * 0.001)
#define STEP_10MS 480           // 10ms步长
#define STEP_1MS 48             // 1ms步长
#define JUMP_250MS 12000        // 250ms跳跃(样本数)

// 状态机状态
typedef enum {
    STATE_IDLE,                // 初始状态，搜索第一个1900Hz
    STATE_FOUND_1900_FIRST,    // 找到第一个1900Hz，准备跳跃
    STATE_SEARCHING_1200,      // 搜索1200Hz信号
    STATE_FOUND_1200,          // 找到1200Hz，搜索第二个1900Hz
    STATE_COMPLETE             // 完整检测到序列
} VIS_State;

// 结果结构
typedef struct {
    int found;                 // 是否找到同步头
    int start_position;        // 第一个1900Hz开始位置
    int sync_position;         // 1200Hz位置(同步基准点)
    int end_position;          // 第二个1900Hz结束位置
} vis_result;

// 函数声明
extern int read_iq_data_range_static_BYTE(FILE* file, int offset, int count, signed char* buffer);
extern void fm_demodulate(signed char* I, signed char* Q, int length, short* freq_fx, int sample_rate);

// 计算频率均值
short calc_freq_avg(short* freq_data, int start, int length) {
    long sum = 0;
    for (int i = 0; i < length; i++) {
        if (start + i < 0) continue;  // 避免越界
        sum += freq_data[start + i];
    }
    //printf("Frequency average: %d\n", (short)(sum / length));
    return (short)(sum / length);
}

// 检查频率是否在目标频率容差范围内
int is_freq_match(short freq, short target_freq) {
    return abs(freq - target_freq) <= FREQ_TOLERANCE;
}

// 状态机检测VIS同步头
vis_result detect_vis_sync(FILE* file_i, FILE* file_q, int search_start, int search_length) {
    vis_result result = {0, -1, -1, -1};
    
    // 分配缓冲区
    signed char* i_buffer = (signed char*)malloc(search_length);
    signed char* q_buffer = (signed char*)malloc(search_length);
    short* freq_data = (short*)malloc(search_length * sizeof(short));
    
    if (!i_buffer || !q_buffer || !freq_data) {
        printf("Memory allocation failed\n");
        if (i_buffer) free(i_buffer);
        if (q_buffer) free(q_buffer);
        if (freq_data) free(freq_data);
        return result;
    }
    
    // 读取IQ数据
    printf("Reading IQ data...\n");
    if (!read_iq_data_range_static_BYTE(file_i, search_start, search_length, i_buffer) ||
        !read_iq_data_range_static_BYTE(file_q, search_start, search_length, q_buffer)) {
        printf("Failed to read IQ data\n");
        free(i_buffer);
        free(q_buffer);
        free(freq_data);
        return result;
    }
    
    // FM解调获取频率数据
    printf("Demodulating to get frequency data...\n");
    fm_demodulate(i_buffer, q_buffer, search_length, freq_data, SAMPLE_RATE);
    
    // 状态机变量
    VIS_State state = STATE_IDLE;
    int position = 0;
    int first_1900_start = -1;
    int first_1900_end = -1;
    int freq_1200_pos = -1;
    int second_1900_start = -1;
    int consecutive_count = 0;  // 连续匹配计数
    
    printf("Starting VIS sync detection...\n");
    
    // 循环处理信号
    while (position + WINDOW_10MS < search_length) {
        switch (state) {
            case STATE_IDLE:
                // 使用10ms窗口和步长搜索第一个1900Hz信号
                {
                    short avg_freq = calc_freq_avg(freq_data, position, WINDOW_10MS);
                    
                    if (is_freq_match(avg_freq, FREQ_1900)) {
                        consecutive_count++;
                        
                        // 需要至少30个连续10ms窗口 (300ms) 的1900Hz
                        if (consecutive_count == 1) {
                            first_1900_start = position;
                            printf("Potential first 1900Hz found at %d (%.3fs)\n", 
                                  position + search_start, (position + search_start)/(float)SAMPLE_RATE);
                        } else if (consecutive_count >= 29) {
                            first_1900_end = position + WINDOW_10MS;
                            state = STATE_FOUND_1900_FIRST;
                            printf("First 1900Hz signal confirmed: start=%d, end=%d (%.3f-%.3fs)\n",
                                  first_1900_start + search_start, 
                                  first_1900_end + search_start,
                                  (first_1900_start + search_start)/(float)SAMPLE_RATE,
                                  (first_1900_end + search_start)/(float)SAMPLE_RATE);
                            position = first_1900_end;  // 跳跃250ms
                            consecutive_count = 0;
                            continue;
                        }
                    } else {
                        // 重置连续计数
                        consecutive_count = 0;
                        first_1900_start = -1;
                    }
                    
                    position += STEP_10MS;
                }
                break;
                
            case STATE_FOUND_1900_FIRST:
                // 切换到使用1ms窗口和步长搜索1200Hz信号
                state = STATE_SEARCHING_1200;
                printf("Searching for 1200Hz signal near %.3fs\n", 
                      (position + search_start)/(float)SAMPLE_RATE);
                break;
                
            case STATE_SEARCHING_1200:
                // 使用1ms窗口搜索1200Hz信号
                {
                    short avg_freq = calc_freq_avg(freq_data, position, WINDOW_1MS);
                    
                    if (is_freq_match(avg_freq, FREQ_1200)) {
                        consecutive_count++;
                        
                        // 需要约10个连续的1ms窗口 (10ms) 的1200Hz
                        if (consecutive_count == 1) {
                            freq_1200_pos = position;
                            printf("Potential 1200Hz found at %d (%.3fs)\n", 
                                  position + search_start, (position + search_start)/(float)SAMPLE_RATE);
                        } else if (consecutive_count >= 9) {
                            state = STATE_FOUND_1200;
                            printf("1200Hz signal confirmed at position %d (%.3fs)\n", 
                                  freq_1200_pos + search_start, (freq_1200_pos + search_start)/(float)SAMPLE_RATE);
                            position = position + WINDOW_1MS;
                            consecutive_count = 0;
                            continue;
                        }
                    } else {
                        // 非1200Hz，重置
                        consecutive_count = 0;
                    }
                    
                    position += STEP_1MS;
                    
                    // 搜索限制，避免过远搜索
                    if (position > first_1900_end + SAMPLE_RATE * 0.5) { // 最多搜索500ms
                        printf("Could not find 1200Hz signal within range, resetting\n");
                        state = STATE_IDLE;
                        position = first_1900_start + STEP_10MS; // 回到初始1900Hz之后一点继续搜索
                        consecutive_count = 0;
                    }
                }
                break;
                
            case STATE_FOUND_1200:
                // 搜索第二个1900Hz信号
                {
                    short avg_freq = calc_freq_avg(freq_data, position, WINDOW_10MS);
                    
                    if (is_freq_match(avg_freq, FREQ_1900)) {
                        consecutive_count++;
                        
                        // 标记第二个1900Hz开始位置
                        if (consecutive_count == 1) {
                            second_1900_start = position;
                            printf("Potential second 1900Hz found at %d (%.3fs)\n", 
                                  position + search_start, (position + search_start)/(float)SAMPLE_RATE);
                        } else if (consecutive_count >= 29) { // 需要至少300ms的1900Hz
                            state = STATE_COMPLETE;
                            int second_1900_end = position + WINDOW_10MS;
                            printf("Second 1900Hz signal confirmed: start=%d, end=%d (%.3f-%.3fs)\n",
                                  second_1900_start + search_start, 
                                  second_1900_end + search_start,
                                  (second_1900_start + search_start)/(float)SAMPLE_RATE,
                                  (second_1900_end + search_start)/(float)SAMPLE_RATE);
                                  
                            // 设置检测结果
                            result.found = 1;
                            result.start_position = first_1900_start + search_start;
                            result.sync_position = freq_1200_pos + search_start;
                            result.end_position = second_1900_end + search_start;
                            break; // 检测完成，退出循环
                        }
                    } else {
                        // 非1900Hz，重置计数
                        consecutive_count = 0;
                    }
                    
                    position += STEP_10MS;
                    
                    // 搜索限制，避免过远搜索
                    if (position > freq_1200_pos + SAMPLE_RATE * 0.5) {
                        printf("Could not find second 1900Hz signal within range, resetting\n");
                        state = STATE_IDLE;
                        position = first_1900_start + STEP_10MS;
                        consecutive_count = 0;
                    }
                }
                break;
                
            case STATE_COMPLETE:
                // 已完成，不应该到达这里
                goto cleanup;
                break;
        }
    }
    cleanup:
        // 释放内存
        free(i_buffer);
        free(q_buffer);
        free(freq_data);
        
        // 输出结果
        if (result.found) {
            printf("\nVIS sync header detected successfully:\n");
            printf("- Start position: %d (%.3fs)\n", result.start_position, result.start_position/(float)SAMPLE_RATE);
            printf("- Sync position (1200Hz): %d (%.3fs)\n", result.sync_position, result.sync_position/(float)SAMPLE_RATE);
            printf("- End position: %d (%.3fs)\n", result.end_position, result.end_position/(float)SAMPLE_RATE);
            printf("- Total duration: %.3f ms\n", (result.end_position - result.start_position) * 1000.0 / SAMPLE_RATE);
        } else {
            printf("\nNo complete VIS sync header found\n");
        }
        
        return result;
}

// 主函数
int main() {
    const char* iq_data_i_path = "./sstv_data_file/iq_i_signal_with_noise_big_endian.bin";
    const char* iq_data_q_path = "./sstv_data_file/iq_q_signal_with_noise_big_endian.bin";
    
    FILE* file_i = fopen(iq_data_i_path, "rb");
    FILE* file_q = fopen(iq_data_q_path, "rb");
    
    if (file_i == NULL || file_q == NULL) {
        printf("Could not open IQ data files\n");
        if (file_i) fclose(file_i);
        if (file_q) fclose(file_q);
        return 1;
    }
    
    // 搜索参数
    int search_start = SAMPLE_RATE * 0;
    int search_length = SAMPLE_RATE * 10;  // 搜索前10秒数据
    
    vis_result result = detect_vis_sync(file_i, file_q, search_start, search_length);
    
    if (result.found) {
        printf("\nSync position (1200Hz location) can be used as reference: %d\n", result.sync_position);
    }
    
    fclose(file_i);
    fclose(file_q);
    return 0;
}