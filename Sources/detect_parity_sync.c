#include "detect_parity_sync.h"

// 奇偶行检测函数 - 在行同步检测完成后调用
parity_result detect_line_parity(FILE* file_i, FILE* file_q, int search_start_point, int search_length_point) {
    parity_result result = {0, 0, -1, 0};
    
    
    
    // 计算分割脉冲起始位置 (行同步位置 + Y扫描时间)
    int pulse_start = search_start_point;
    
    // 分配内存
    short* i_buffer = (short*)malloc(search_length_point * sizeof(short));
    short* q_buffer = (short*)malloc(search_length_point * sizeof(short));
    short* freq_data = (short*)malloc(search_length_point * sizeof(short));
    
    if (!i_buffer || !q_buffer || !freq_data) {
        printf("Memory allocation failed in detect_line_parity\n");
        if (i_buffer) free(i_buffer);
        if (q_buffer) free(q_buffer);
        if (freq_data) free(freq_data);
        return result;
    }
    
    // 读取分割脉冲IQ数据
    if (!read_iq_data_range_static(file_i, pulse_start, search_length_point, i_buffer) ||
        !read_iq_data_range_static(file_q, pulse_start, search_length_point, q_buffer)) {
        printf("Failed to read pulse IQ data at position %d\n", pulse_start);
        free(i_buffer);
        free(q_buffer);
        free(freq_data);
        return result;
    }
    
    // 解调计算频率
    fm_demodulate_fx(i_buffer, q_buffer, search_length_point, freq_data, SAMPLE_RATE);
    
    // 计算频率均值
    long freq_sum = 0;
    for (int i = 0; i < search_length_point; i++) {
        freq_sum += freq_data[i];
    }
    short avg_freq = (short)(freq_sum / search_length_point);
    
    // 判断奇偶性 - 分割脉冲频率：偶数行≈1500Hz，奇数行≈2300Hz
    int is_odd = abs(avg_freq - FREQ_2300) < abs(avg_freq - FREQ_1500);
    
    // 计算门廊信号开始位置
    int porch_start = pulse_start + search_length_point;
    
    // 验证门廊信号是否真的是1900Hz (可选)
    // 这里可以添加代码检查门廊信号的频率，但简化起见，我们假设它总是紧跟在分割脉冲之后
    
    // 设置检测结果
    result.found = 1;
    result.is_odd = is_odd;
    result.sync_position = porch_start;  // 设置同步点为门廊信号起始点
    result.pulse_frequency = avg_freq;
    
    // 释放内存
    free(i_buffer);
    free(q_buffer);
    free(freq_data);
    
    // 输出信息
    printf("Line parity detection result:\n");
    printf("- Line type: %s\n", result.is_odd ? "ODD" : "EVEN");
    printf("- Pulse frequency: %d Hz\n", result.pulse_frequency);
    printf("- Porch (sync) position: %d (%.3fs)\n", 
           result.sync_position, 
           result.sync_position/(float)SAMPLE_RATE);
    
    return result;
}

// 使用封装函数，简化调用
parity_result detect_line_parity_use(FILE* file_i, FILE* file_q, int search_start_point, int search_length_point) {
    
    parity_result result = detect_line_parity(file_i, file_q, search_start_point, search_length_point);

    if (result.found) {
        printf("\nSync position (1900Hz location) can be used as reference: %d\n", result.sync_position);
    }
    return result;
}