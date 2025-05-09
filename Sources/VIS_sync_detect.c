#include "vis_sync.h"
#include <string.h>
#include "sbdsp/sbatan.h"

// 外部函数声明
extern int read_iq_data_range_static(FILE* file, int offset, int count, short* buffer);
extern void fm_demodulate_fx(short* I, short* Q, int length, short* freq_fx, int sample_rate);

// 创建VIS同步头模板
static void create_vis_template(int sample_rate, short** template_data, int* template_length) {
    // 计算各部分样本数
    int first_samples = (VIS_SYNC_FIRST_DURATION * sample_rate) / 1000;
    int break_samples = (VIS_SYNC_BREAK_DURATION * sample_rate) / 1000;
    int second_samples = (VIS_SYNC_SECOND_DURATION * sample_rate) / 1000;
    
    // 分配模板内存
    *template_length = first_samples + break_samples + second_samples;
    *template_data = (short*)malloc(*template_length * sizeof(short));
    
    if (*template_data == NULL) {
        printf("无法分配模板内存\n");
        *template_length = 0;
        return;
    }
    
    // 创建模板 (存储频率值)
    for (int i = 0; i < *template_length; i++) {
        if (i < first_samples || i >= (first_samples + break_samples)) {
            (*template_data)[i] = VIS_SYNC_HIGH_FREQ; // 1900Hz部分
        } else {
            (*template_data)[i] = VIS_SYNC_LOW_FREQ;  // 1200Hz部分
        }
    }
}

// 计算归一化互相关系数
static float compute_correlation(short* signal, short* template_data, int length) {
    double sum = 0.0;
    double signal_energy = 0.0;
    double template_energy = 0.0;
    
    for (int i = 0; i < length; i++) {
        sum += (double)signal[i] * template_data[i];
        signal_energy += (double)signal[i] * signal[i];
        template_energy += (double)template_data[i] * template_data[i];
    }
    
    // 防止除零
    if (signal_energy <= 0 || template_energy <= 0) {
        return 0.0f;
    }
    
    // 归一化互相关
    return (float)(sum / sqrt(signal_energy * template_energy));
}

// 进行频谱分析检测特定频率的能量
static void analyze_spectrum(short* freq_data, int length, int target_freq, int sample_rate, float* energy_array, int* array_length) {
    int frame_len = 1024;  // 每帧长度
    
    // 计算帧数
    int num_frames = length / frame_len;
    *array_length = num_frames;
    
    // 计算目标频率对应的FFT bin索引 (近似)
    int target_bin = (target_freq * frame_len) / sample_rate;
    
    // 计算每个帧的目标频率能量
    for (int i = 0; i < num_frames; i++) {
        float sum_energy = 0.0f;
        int bin_width = 3;  // 取目标频率周围的几个bin
        int count = 0;
        
        for (int j = -bin_width/2; j <= bin_width/2; j++) {
            int bin = target_bin + j;
            if (bin >= 0 && bin < frame_len) {
                // 获取该频率的瞬时值，求平方近似能量
                short val = freq_data[i * frame_len + bin];
                sum_energy += (float)(val * val);
                count++;
            }
        }
        
        energy_array[i] = (count > 0) ? (sum_energy / count) : 0.0f;
    }
}

// VIS同步头检测实现
vis_sync_result detect_vis_header(FILE* file_i, FILE* file_q, int search_start, int search_length) {
    vis_sync_result result = {0, -1, -1, 0.0f};
    
    // 步骤1: 分配缓冲区
    short* i_buffer = (short*)malloc(search_length * sizeof(short));
    short* q_buffer = (short*)malloc(search_length * sizeof(short));
    short* freq_data = (short*)malloc(search_length * sizeof(short));
    
    if (!i_buffer || !q_buffer || !freq_data) {
        printf("内存分配失败\n");
        if (i_buffer) free(i_buffer);
        if (q_buffer) free(q_buffer);
        if (freq_data) free(freq_data);
        return result;
    }
    
    // 步骤2: 读取IQ数据
    printf("读取IQ数据...\n");
    if (!read_iq_data_range_static(file_i, search_start, search_length, i_buffer) ||
        !read_iq_data_range_static(file_q, search_start, search_length, q_buffer)) {
        printf("无法读取IQ数据\n");
        free(i_buffer);
        free(q_buffer);
        free(freq_data);
        return result;
    }
    
    // 步骤3: FM解调获取频率数据
    printf("执行FM解调...\n");
    fm_demodulate_fx(i_buffer, q_buffer, search_length, freq_data, SAMPLE_RATE);
    
    // 步骤4: 第一阶段 - 粗略检测1900Hz频率段落
    printf("正在进行频谱分析寻找VIS头...\n");
    float* energy_array = (float*)malloc((search_length/1024) * sizeof(float));
    int array_length = 0;
    
    if (!energy_array) {
        printf("内存分配失败\n");
        free(i_buffer);
        free(q_buffer);
        free(freq_data);
        return result;
    }
    
    analyze_spectrum(freq_data, search_length, VIS_SYNC_HIGH_FREQ, SAMPLE_RATE, energy_array, &array_length);
    
    // 寻找连续高能量的1900Hz段落
    float max_energy = 0.0f;
    int max_energy_start = -1;
    int min_consecutive = 4;  // 至少需要连续几帧能量高
    
    for (int i = 0; i < array_length - min_consecutive; i++) {
        float sum_energy = 0.0f;
        for (int j = 0; j < min_consecutive; j++) {
            sum_energy += energy_array[i+j];
        }
        
        if (sum_energy > max_energy) {
            max_energy = sum_energy;
            max_energy_start = i;
        }
    }
    
    free(energy_array);
    
    if (max_energy_start < 0) {
        printf("未检测到强1900Hz信号段\n");
        free(i_buffer);
        free(q_buffer);
        free(freq_data);
        return result;
    }
    
    // 粗略位置
    int rough_pos = max_energy_start * 1024;
    printf("找到可能的VIS同步头位置: %d\n", search_start + rough_pos);
    
    // 步骤5: 第二阶段 - 精确模板匹配
    // 创建VIS同步头模板
    short* template_data = NULL;
    int template_length = 0;
    create_vis_template(SAMPLE_RATE, &template_data, &template_length);
    
    if (!template_data || template_length == 0) {
        printf("创建VIS模板失败\n");
        free(i_buffer);
        free(q_buffer);
        free(freq_data);
        return result;
    }
    
    // 在粗略位置附近进行精确匹配
    int search_window = SAMPLE_RATE * 2;  // 搜索2秒的窗口
    int refine_start = rough_pos - search_window/2;
    if (refine_start < 0) refine_start = 0;
    
    int refine_length = search_window;
    if (refine_start + refine_length > search_length) {
        refine_length = search_length - refine_start;
    }
    
    // 计算所有可能位置的互相关
    float best_corr = 0.0f;
    int best_pos = -1;
    
    printf("进行模板匹配...\n");
    for (int i = 0; i <= refine_length - template_length; i++) {
        float corr = compute_correlation(&freq_data[refine_start + i], template_data, template_length);
        
        if (corr > best_corr) {
            best_corr = corr;
            best_pos = refine_start + i;
        }
    }
    
    // 设置检测结果
    if (best_corr >= VIS_CORR_THRESHOLD) {
        result.found = 1;
        result.start_position = search_start + best_pos;
        result.data_position = result.start_position + template_length;
        result.confidence = best_corr;
        
        printf("成功检测到VIS同步头: 位置 = %d, 置信度 = %.2f%%\n", 
               result.start_position, result.confidence * 100.0f);
    } else {
        printf("未检测到有效的VIS同步头。最佳匹配: 位置 = %d, 置信度 = %.2f%%\n",
               search_start + best_pos, best_corr * 100.0f);
    }
    
    // 释放资源
    free(i_buffer);
    free(q_buffer);
    free(freq_data);
    free(template_data);
    
    return result;
}