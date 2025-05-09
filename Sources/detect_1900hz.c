#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include "sbdsp/sbfft.h"

#define SAMPLE_RATE 48000       // 原始采样率为48kHz
#define DOWN_SAMPLE_RATE 8000   // 下采样率
#define DOWN_NUM (SAMPLE_RATE / DOWN_SAMPLE_RATE) // 下采样间隔采样点数
#define FFT_SIZE 256            // FFT大小
#define WINDOW_OVERLAP 128      // 窗口重叠点数

// 目标频率以及搜索范围
#define TARGET_FREQ 1900        // 目标频率(Hz)
#define FREQ_TOLERANCE 100      // 频率容差(Hz)

// 检测参数
#define MIN_ENERGY_THRESHOLD 100  // 最小能量阈值
#define CONSECUTIVE_WINDOWS 3     // 需要连续检测到的窗口数

// 结果结构
typedef struct {
    int found;                  // 是否找到高能量信号
    int start_position;         // 开始位置(原始采样率下的样本索引)
    int end_position;           // 结束位置
    short max_energy;           // 最大能量值
} detection_result;

// 函数声明
int read_iq_data_range_static_BYTE(FILE* file, int offset, int count, signed char* buffer);
detection_result detect_1900hz_signal(FILE* file_i, FILE* file_q, int search_start, int search_length);
void calculate_signal_fft(signed char* i_data, signed char* q_data, short* fft_out, int window_size);
short get_frequency_energy(short* fft_out, short target_bin, short bin_width);

int read_iq_data_range_static_BYTE(FILE* file, int offset, int count, signed char* buffer) {
    if (file == NULL) {
        printf("文件指针为空\n");
        return 0;
    }

    long point = (long)(offset * sizeof(signed char));
    //long point = (long)0;
    // 定位到文件中的指定位置
    if (fseek(file, point, SEEK_SET) != 0) {
        printf("文件定位失败，偏移量: %ld\n", offset);
        return 0;
    }

    // 读取指定数量的数据到提供的缓冲区
    size_t read_count = fread(buffer, sizeof(signed char), count, file);
    //*(buffer) = (*(buffer) & 0x00FF) << 8 | (*(buffer) & 0xFF00) >> 8;
    if (read_count != count) {
        printf("读取数据失败: 预期 %d 项，实际读取 %zu 项\n", count, read_count);
        return 0;
    }

    return 1;
}

// 信号FFT分析及能量计算
void calculate_signal_fft(signed char* i_data, signed char* q_data, short* fft_out, int window_size) {
    short fft_data[FFT_SIZE * 2];  // FFT输入缓冲区
    
    // 准备FFT输入数据
    for (int i = 0; i < window_size; i++) {
        fft_data[2 * i] = (short)i_data[DOWN_NUM * i];     // 实部
        fft_data[2 * i + 1] = (short)q_data[DOWN_NUM * i]; // 虚部
    }
    
    // 执行FFT
    sbfft256(fft_data, fft_out);
}

// 获取指定频率附近的能量
short get_frequency_energy(short* fft_out, short target_bin, short bin_width) {
    int total_energy = 0;
    short count = 0;
    
    // 计算目标频率附近的bin范围
    short start_bin = target_bin - bin_width/2;
    short end_bin = target_bin + bin_width/2;
    
    // 确保范围有效
    if (start_bin < 0) start_bin = 0;
    if (end_bin >= FFT_SIZE) end_bin = FFT_SIZE - 1;
    
    // 计算指定频率范围的平均能量
    for (int i = start_bin; i <= end_bin; i++) {
        short real = fft_out[2 * i];
        short imag = fft_out[2 * i + 1];
        short bin_energy = sqrt(real*real + imag*imag) / FFT_SIZE;
        
        total_energy += bin_energy;
        count++;
    }
    
    return (count > 0) ? (short)(total_energy) : 0;
}

// 高能量1900Hz信号检测
detection_result detect_1900hz_signal(FILE* file_i, FILE* file_q, int search_start, int search_length) {
    detection_result result = {0, -1, -1, 0};
    
    // 计算需要的窗口数
    short step_size = FFT_SIZE - WINDOW_OVERLAP;  //128
    short num_windows = (search_length / DOWN_NUM - FFT_SIZE) / step_size + 1;    //624
    
    if (num_windows <= 0) {
        printf("搜索范围太小，无法进行有效检测\n");
        return result;
    }
    
    // 分配数据缓冲区 - 读取整个搜索范围
    int buffer_size = search_length;
    signed char I_buffer[buffer_size];
    signed char Q_buffer[buffer_size];
    
    
    // 读取整个搜索范围的IQ数据
    printf("读取IQ数据...\n");
    if (!read_iq_data_range_static_BYTE(file_i, search_start, buffer_size, I_buffer) ||
        !read_iq_data_range_static_BYTE(file_q, search_start, buffer_size, Q_buffer)) {
        printf("读取IQ数据失败\n");
        return result;
    }
    
    // 计算目标频率对应的FFT bin
    short target_bin = (TARGET_FREQ * FFT_SIZE) / DOWN_SAMPLE_RATE;
    short bin_width = (2 * FREQ_TOLERANCE * FFT_SIZE) / DOWN_SAMPLE_RATE;
    
    printf("1900Hz对应FFT bin: %d, 宽度: %d\n", target_bin, bin_width);
    
    // FFT输出缓冲区
    short fft_out[FFT_SIZE * 2]; // FFT输出缓冲区，大小为FFT_SIZE * 2,实数部分和虚数部分交替存储
    
    // 存储每个窗口的能量值
    short energy_values[num_windows]; // 能量值数组
    
    // 对每个窗口进行FFT分析
    printf("执行滑动窗口FFT分析...\n");
    for (int w = 0; w < num_windows; w++) {
        int window_start = w * step_size * DOWN_NUM;
        
        // 使用窗口数据计算FFT
        calculate_signal_fft(&I_buffer[window_start], &Q_buffer[window_start], fft_out, FFT_SIZE);
        
        // 计算目标频率附近的能量
        energy_values[w] = get_frequency_energy(fft_out, target_bin, bin_width);
        
        // 打印进度
        if (w % 10 == 0) {
            printf("\r分析窗口进度: %d/%d", w, num_windows);
        }
    }
    printf("\r分析窗口进度: %d/%d\n", num_windows, num_windows);
    
    // 查找连续的高能量窗口
    int consecutive_count = 0;
    int best_start_window = -1;
    int best_end_window = -1;
    short max_energy = 0;
    
    for (int w = 0; w < num_windows; w++) {
        if (energy_values[w] > MIN_ENERGY_THRESHOLD) {
            consecutive_count++;
            
            // 记录连续窗口的最大能量
            if (energy_values[w] > max_energy) {
                max_energy = energy_values[w];
            }
            
            // 如果满足连续窗口条件
            if (consecutive_count >= CONSECUTIVE_WINDOWS) {
                int segment_start = w - consecutive_count + 1;
                
                // 如果是第一次找到或者能量更高，更新结果
                if (best_start_window < 0 || max_energy > result.max_energy) {
                    best_start_window = segment_start;
                    best_end_window = w;
                    result.max_energy = max_energy;
                }
            }
        } else {
            // 重置连续计数
            consecutive_count = 0;
            max_energy = 0;
        }
    }
    
    // 如果找到高能量区域
    if (best_start_window >= 0) {
        // 转换为原始样本位置
        result.found = 1;
        result.start_position = search_start + best_start_window * step_size * DOWN_NUM;
        result.end_position = search_start + (best_end_window + 1) * step_size * DOWN_NUM;
        
        printf("检测到1900Hz高能量信号:\n");
        printf("  开始位置: %d\n", result.start_position);
        printf("  结束位置: %d\n", result.end_position);
        printf("  持续时间: %.3f秒\n", (result.end_position - result.start_position) / (float)SAMPLE_RATE);
        printf("  最大能量: %d\n", result.max_energy);
    } else {
        printf("未检测到满足条件的1900Hz高能量信号\n");
    }
    
    return result;
}

// 主函数示例
int main() {
    const char* iq_data_i_path = "./sstv_data_file/sstv_iq_i_scaled_big_endian.bin";
    const char* iq_data_q_path = "./sstv_data_file/sstv_iq_q_scaled_big_endian.bin";
    
    FILE* file_i = fopen(iq_data_i_path, "rb");
    FILE* file_q = fopen(iq_data_q_path, "rb");
    
    if (file_i == NULL || file_q == NULL) {
        printf("无法打开IQ数据文件\n");
        if (file_i) fclose(file_i);
        if (file_q) fclose(file_q);
        return 1;
    }
    
    // 搜索参数
    int search_start = 0;
    int search_length = SAMPLE_RATE * 10;  // 搜索前10秒数据
    
    printf("开始搜索1900Hz高能量信号...\n");
    detection_result result = detect_1900hz_signal(file_i, file_q, search_start, search_length);
    
    if (result.found) {
        printf("成功检测到1900Hz高能量信号段!\n");
        // 后续处理...
    }
    
    fclose(file_i);
    fclose(file_q);
    return 0;
}
