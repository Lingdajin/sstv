#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

// 定义常量
#define SAMPLE_RATE 48000       // 原始采样率为48kHz
#define TARGET_FREQ 1900        // 目标频率(Hz)
#define TEMPLATE_DURATION_MS 20 // 模板持续时间(ms)
#define SEARCH_EXTEND_MS 100    // 搜索范围扩展时间(ms)

// 导入检测结果结构体定义
typedef struct {
    int found;                  // 是否找到高能量信号
    int start_position;         // 开始位置(原始采样率下的样本索引)
    int end_position;           // 结束位置
    short max_energy;           // 最大能量值
} detection_result;

// 函数声明
extern int read_iq_data_range_static_BYTE(FILE* file, int offset, int count, signed char* buffer);
extern void fm_demodulate(signed char* I, signed char* Q, int length, short* freq_fx, int sample_rate);

// 优化的互相关计算 - 直接找出最大互相关点的位置
int calculate_correlation_maxpos(short* signal, int signal_length,
                                short* template_data, int template_length) {
    // 计算模板的均值
    int template_sum = 0;
    for (int j = 0; j < template_length; j++) {
        template_sum += template_data[j];
    }
    int template_mean = template_sum / template_length;
    
    // 预先减去模板的均值并存储
    short centered_template[template_length];
    for (int j = 0; j < template_length; j++) {
        centered_template[j] = template_data[j] - template_mean;
    }
    
    // 计算互相关，但只记录最大值位置
    int corr_length = signal_length - template_length + 1;
    int max_pos = 0;
    long long max_corr = -9223372036854775807LL; // 最小的long long值
    
    // 初始化第一个窗口的信号和
    int signal_sum = 0;
    for (int j = 0; j < template_length; j++) {
        signal_sum += signal[j];
    }
    
    // 滑动窗口计算互相关
    for (int i = 0; i < corr_length; i++) {
        // 计算当前窗口的信号均值
        int signal_mean = signal_sum / template_length;
        
        // 计算互相关值（去均值后）
        long long sum_corr = 0;
        for (int j = 0; j < template_length; j++) {
            int signal_val = signal[i+j] - signal_mean;
            sum_corr += (long long)signal_val * centered_template[j];
        }
        
        // 更新最大值
        if (sum_corr > max_corr) {
            max_corr = sum_corr;
            max_pos = i;
        }
        
        // 更新信号和（增量方式）
        if (i < corr_length - 1) {
            signal_sum = signal_sum - signal[i] + signal[i + template_length];
        }
    }
    
    return max_pos;
}

// 优化后的1900Hz信号起点检测函数
int detect_1900hz_start(FILE* file_i, FILE* file_q, detection_result* result) {
    if (!result->found) {
        printf("未检测到信号，无法定位起点\n");
        return 0;
    }
    
    // 扩展搜索范围 - 主要向前扩展
    int extend_samples = SAMPLE_RATE * SEARCH_EXTEND_MS / 1000;  // 扩展100ms
    int search_start = result->start_position - extend_samples;
    if (search_start < 0) search_start = 0;
    
    int search_length = extend_samples * 2 + SAMPLE_RATE * 0.1;  // 向后也略微扩展一点
    
    printf("进行互相关分析，搜索范围: %.3fs ~ %.3fs\n",
           search_start/(float)SAMPLE_RATE, (search_start+search_length)/(float)SAMPLE_RATE);
    
    // 分配数据缓冲区
    signed char* i_buffer = (signed char*)malloc(search_length);
    signed char* q_buffer = (signed char*)malloc(search_length);
    short* freq_data = (short*)malloc(search_length * sizeof(short));
    
    if (!i_buffer || !q_buffer || !freq_data) {
        printf("内存分配失败\n");
        if (i_buffer) free(i_buffer);
        if (q_buffer) free(q_buffer);
        if (freq_data) free(freq_data);
        return 0;
    }
    
    // 读取IQ数据
    if (!read_iq_data_range_static_BYTE(file_i, search_start, search_length, i_buffer) ||
        !read_iq_data_range_static_BYTE(file_q, search_start, search_length, q_buffer)) {
        printf("读取IQ数据失败\n");
        free(i_buffer);
        free(q_buffer);
        free(freq_data);
        return 0;
    }
    
    // FM解调获得频率数据
    fm_demodulate(i_buffer, q_buffer, search_length, freq_data, SAMPLE_RATE);
    
    // 创建1900Hz模板 - 20ms长度
    int template_length = SAMPLE_RATE * TEMPLATE_DURATION_MS / 1000;
    short template_data[template_length];
    
    // 填充模板值
    for (int i = 0; i < template_length; i++) {
        template_data[i] = TARGET_FREQ;  // 1900Hz
    }
    
    // 直接计算互相关最大值位置
    int max_pos = calculate_correlation_maxpos(freq_data, search_length, template_data, template_length);
    
    // 更新结果的起点位置
    int precise_start_pos = search_start + max_pos;
    
    printf("1900Hz信号起点: %d (%.3fs)\n", 
           precise_start_pos, precise_start_pos/(float)SAMPLE_RATE);
    
    // 更新结果
    result->start_position = precise_start_pos;
    
    // 释放内存
    free(i_buffer);
    free(q_buffer);
    free(freq_data);
    
    return 1;  // 成功
}

// 主函数示例
// int main() {
//     const char* iq_data_i_path = "./sstv_data_file/iq_i_signal_with_noise_big_endian.bin";
//     const char* iq_data_q_path = "./sstv_data_file/iq_q_signal_with_noise_big_endian.bin";
    
//     FILE* file_i = fopen(iq_data_i_path, "rb");
//     FILE* file_q = fopen(iq_data_q_path, "rb");
    
//     if (file_i == NULL || file_q == NULL) {
//         printf("无法打开IQ数据文件\n");
//         if (file_i) fclose(file_i);
//         if (file_q) fclose(file_q);
//         return 1;
//     }
    
//     // 假设之前用FFT已经粗略定位结果
//     detection_result result = {1, 239616, 246528, 151};  // 示例值
    
//     detect_1900hz_start(file_i, file_q, &result);
    
//     printf("1900Hz信号起点: %d (%.3f秒)\n", 
//            result.start_position, result.start_position/(float)SAMPLE_RATE);
    
//     fclose(file_i);
//     fclose(file_q);
//     return 0;
// }