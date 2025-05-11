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
#define MIN_ENERGY_THRESHOLD 127  // 最小能量阈值
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
// 添加对FM解调函数的声明
void fm_demodulate(signed char* I, signed char* Q, int length, short* freq_fx, int sample_rate);
void precise_boundary_correlation(FILE* file_i, FILE* file_q, detection_result* result);


// 主函数
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
    
//     // 搜索参数
//     int search_start = 0;
//     int search_length = SAMPLE_RATE * 5.3;
    
//     printf("开始搜索1900Hz高能量信号...\n");
//     detection_result result = detect_1900hz_signal(file_i, file_q, search_start, search_length);
    
//     if (result.found) {
//         printf("成功检测到1900Hz高能量信号段，现在进行精确定位...\n");
        
//         // 使用互相关进行精确定位
//         precise_boundary_correlation(file_i, file_q, &result);
        
//         // 继续使用精确定位后的结果
//         // ...
//     }
    
//     // ... 后续处理 ...
    
//     fclose(file_i);
//     fclose(file_q);
//     return 0;
// }

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
    
    // 计算指定频率范围的能量
    for (int i = start_bin; i <= end_bin; i++) {
        short real = fft_out[2 * i];
        short imag = fft_out[2 * i + 1];
        short real_conjugate = fft_out[2 * (FFT_SIZE - i) - 1];
        short imag_conjugate = fft_out[2 * (FFT_SIZE - i) - 2];
        short bin_energy = sqrt(real*real + imag*imag + real_conjugate*real_conjugate + imag_conjugate*imag_conjugate) / FFT_SIZE;
        
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

void fm_demodulate(signed char* I, signed char* Q, int length, short* freq_fx, int sample_rate) {
    short fx_phase[length];       // 使用静态数组避免栈溢出
    int fx_unwrapped[length];


    // 计算每个采样点的相位 - 使用SB3500平台提供的sb_fxatan函数
    for(int i = 0; i < length; i++) {
        // sb_fxatan输出范围是-32768到32767，对应-π到π
        fx_phase[i] = sb_fxatan(I[i], Q[i]);
    }

    // 相位解包装 - 确保相位连续
    fx_unwrapped[0] = fx_phase[0];
    for(int i = 1; i < length; i++) {
        int diff = fx_phase[i] - fx_phase[i-1];

        // 处理相位跳变 (sb_fxatan输出中π对应32767)
        if(diff > 32767) diff -= 2 * 32767;
        else if(diff < -32767) diff += 2 * 32767;

        fx_unwrapped[i] = fx_unwrapped[i-1] + diff;
    }

    // 计算频率 - 相位差分
    // freq = (phase_diff * sample_rate) / (2*π)
    // 由于sb_fxatan输出已经归一化为-π到π的范围，我们使用其比例因子
    for(int i = 0; i < length - 1; i++) {
        // 相位差
        int phase_diff = fx_unwrapped[i+1] - fx_unwrapped[i];

        // 频率计算: f = (Δφ/2π) * sample_rate
        // 这里我们需要将相位差除以2π对应的sb_fxatan值(2*32767)，然后乘以采样率
        freq_fx[i] = (phase_diff * sample_rate) / (2 * 32767);
        //printf("freq_fx[%d]: %d\n", i, freq_fx[i]);
    }
}

// 使用整数互相关精确定位信号边界
void precise_boundary_correlation(FILE* file_i, FILE* file_q, detection_result* result) {
    if (!result->found) {
        printf("未检测到信号，无法精确定位边界\n");
        return;
    }
    
    // 扩展搜索范围，确保捕获完整边界
    int extend_samples = SAMPLE_RATE * 0.1; // 前后各扩展100ms
    int precise_start = result->start_position - extend_samples;
    if (precise_start < 0) precise_start = 0;
    
    int search_length = (result->end_position - result->start_position) + 2 * extend_samples;
    
    printf("进行整数互相关精确定位，搜索范围: %d ~ %d (%.3fs ~ %.3fs)\n",
           precise_start, precise_start + search_length,
           precise_start / (float)SAMPLE_RATE, (precise_start + search_length) / (float)SAMPLE_RATE);
    
    // 分配数据缓冲区
    signed char* i_buffer = (signed char*)malloc(search_length * sizeof(signed char));
    signed char* q_buffer = (signed char*)malloc(search_length * sizeof(signed char));
    short* freq_data = (short*)malloc(search_length * sizeof(short));
    
    if (!i_buffer || !q_buffer || !freq_data) {
        printf("内存分配失败\n");
        if (i_buffer) free(i_buffer);
        if (q_buffer) free(q_buffer);
        if (freq_data) free(freq_data);
        return;
    }
    
    // 读取扩展范围内的IQ数据
    if (!read_iq_data_range_static_BYTE(file_i, precise_start, search_length, i_buffer) ||
        !read_iq_data_range_static_BYTE(file_q, precise_start, search_length, q_buffer)) {
        printf("读取IQ数据失败\n");
        free(i_buffer);
        free(q_buffer);
        free(freq_data);
        return;
    }
    
    // FM解调获得频率数据
    fm_demodulate(i_buffer, q_buffer, search_length, freq_data, SAMPLE_RATE);
    
    // 创建1900Hz信号模板 - 纯整数
    int template_length = SAMPLE_RATE * 0.02; // 20ms模板
    short* template_data = (short*)malloc(template_length * sizeof(short));
    
    // 创建1900Hz恒定频率模板
    for (int i = 0; i < template_length; i++) {
        template_data[i] = TARGET_FREQ; // 1900Hz
    }
    
    // 计算互相关 - 完全使用整数运算
    int corr_length = search_length - template_length + 1;
    int* correlation = (int*)malloc(corr_length * sizeof(int));
    
    if (!correlation) {
        printf("互相关缓冲区内存分配失败\n");
        free(i_buffer);
        free(q_buffer);
        free(freq_data);
        free(template_data);
        return;
    }
    
    // 计算互相关 - 不归一化，只寻找最大相关性位置
    printf("计算整数互相关...\n");
    
    // 减去信号和模板均值，提高互相关性能
    int signal_sum = 0;
    int template_sum = 0;
    
    // 计算模板的均值
    for (int j = 0; j < template_length; j++) {
        template_sum += template_data[j];
    }
    int template_mean = template_sum / template_length;
    
    for (int i = 0; i < corr_length; i++) {
        // 对于每个窗口，计算信号段的均值
        if (i == 0) {
            // 第一个窗口完整计算均值
            signal_sum = 0;
            for (int j = 0; j < template_length; j++) {
                signal_sum += freq_data[j];
            }
        } else {
            // 增量更新均值，提高效率
            signal_sum = signal_sum - freq_data[i-1] + freq_data[i+template_length-1];
        }
        int signal_mean = signal_sum / template_length;
        
        // 计算零均值互相关
        long long sum_corr = 0; // 使用长整形防止溢出
        for (int j = 0; j < template_length; j++) {
            int signal_val = freq_data[i+j] - signal_mean;
            int template_val = template_data[j] - template_mean;
            sum_corr += (long long)signal_val * template_val;
        }
        
        correlation[i] = (int)sum_corr;
        
        // 打印进度
        if (i % 1000 == 0) {
            printf("\r互相关计算进度: %.1f%%", (i * 100.0f) / corr_length);
        }
    }
    printf("\r互相关计算完成: 100%%\n");
    
    // 找到互相关最大值位置
    int max_corr = 0;
    int max_corr_pos = -1;
    
    for (int i = 0; i < corr_length; i++) {
        if (correlation[i] > max_corr) {
            max_corr = correlation[i];
            max_corr_pos = i;
        }
    }
    
    printf("最大互相关值: %d 位置: %d (%.3fs)\n", 
           max_corr, max_corr_pos, (precise_start + max_corr_pos) / (float)SAMPLE_RATE);
    
    // 计算阈值 - 使用最大值的一定比例
    int threshold = max_corr / 2; // 50%阈值
    
    // 从最大相关位置向前搜索信号开始
    int signal_start = max_corr_pos;
    while (signal_start > 0 && correlation[signal_start] > threshold) {
        signal_start--;
    }
    // 回退一步，因为我们已经越过了阈值
    signal_start++;
    
    // 向后搜索信号结束
    int signal_end = max_corr_pos;
    while (signal_end < corr_length-1 && correlation[signal_end] > threshold) {
        signal_end++;
    }
    signal_end--;
    
    // 寻找1900Hz信号后的下降沿
    int drop_threshold = threshold / 2;
    int boundary_end = signal_end;
    
    // 在信号结束后搜索互相关值显著下降的位置
    for (int i = signal_end + 1; i < corr_length && i <= signal_end + SAMPLE_RATE * 0.05; i++) {
        if (correlation[i] < drop_threshold) {
            boundary_end = i;
            break;
        }
    }
    
    // 可视化互相关结果（打印简化图表）
    printf("\n互相关结果可视化 (从位置 %d 开始):\n", 
           (max_corr_pos > 500) ? max_corr_pos - 500 : 0);
           
    int viz_start = (max_corr_pos > 500) ? max_corr_pos - 500 : 0;
    int viz_end = (viz_start + 1000 < corr_length) ? viz_start + 1000 : corr_length - 1;
    
    // 计算可视化比例因子
    int max_bar_len = 50;
    int scale_factor = (max_corr > max_bar_len) ? (max_corr / max_bar_len) : 1;
    
    for (int i = viz_start; i <= viz_end; i += 10) { // 每10个点绘制一次以减少输出
        if (i == signal_start) printf("[");
        else if (i == signal_end) printf("]");
        else if (i == boundary_end) printf(">");
        else printf(" ");
        
        int bar_len = correlation[i] / scale_factor;
        if (bar_len > max_bar_len) bar_len = max_bar_len;
        if (bar_len < 0) bar_len = 0;
        
        printf("%5d: ", i);
        for (int j = 0; j < bar_len; j++) printf("#");
        printf(" %d\n", correlation[i]);
    }
    
    // 更新精确边界位置
    if (signal_start >= 0) {
        // 转换为原始信号中的位置
        int precise_start_pos = precise_start + signal_start;
        int precise_end_pos = precise_start + boundary_end;
        
        printf("\n精确定位结果:\n");
        printf("  信号起始点: %d (%.3fs) [原始: %d]\n", 
               precise_start_pos, precise_start_pos / (float)SAMPLE_RATE, result->start_position);
        printf("  信号结束点: %d (%.3fs) [原始: %d]\n", 
               precise_end_pos, precise_end_pos / (float)SAMPLE_RATE, result->end_position);
        printf("  信号持续时间: %.3f秒\n", (precise_end_pos - precise_start_pos) / (float)SAMPLE_RATE);
        
        // 更新结果
        result->start_position = precise_start_pos;
        result->end_position = precise_end_pos;
    } else {
        printf("未能通过互相关找到清晰的信号边界\n");
    }
    
    // 释放内存
    free(i_buffer);
    free(q_buffer);
    free(freq_data);
    free(template_data);
    free(correlation);
}