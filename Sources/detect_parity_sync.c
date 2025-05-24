#include "detect_parity_sync.h"


parity_result detect_line_parity(FILE* file_i, FILE* file_q, int search_start, int search_length) {
    parity_result result = {0, 0, -1, 0};
    
    // 状态机变量
    Parity_State state = PARITY_STATE_IDLE;
    int position = 0;
    int first_1500_start = -1;
    int first_1500_end = -1;
    int first_2300_start = -1;
    int first_2300_end = -1;
    int freq_1900_pos = -1;
    int freq_1900_end = -1;
    int consecutive_count = 0;  // 连续匹配计数
    int detection_completed = 0;  // 检测完成标志
    
    printf("Starting line parity detection...\n");
    
    // 循环处理信号，每个状态都有自己的缓冲区
    while (position + WINDOW_250US < search_length && !detection_completed) {
        switch (state) {
            case PARITY_STATE_IDLE:
                // 使用0.25ms窗口和步长搜索第一个1500Hz信号
                {
                    // 为当前状态分配内存
                    short* i_buffer = (short*)malloc(WINDOW_250US * sizeof(short));
                    short* q_buffer = (short*)malloc(WINDOW_250US * sizeof(short));
                    short* freq_data = (short*)malloc(WINDOW_250US * sizeof(short));
                    
                    if (!i_buffer || !q_buffer || !freq_data) {
                        printf("Memory allocation failed in STATE_IDLE\n");
                        if (i_buffer) free(i_buffer);
                        if (q_buffer) free(q_buffer);
                        if (freq_data) free(freq_data);
                        return result;
                    }
                    
                    // 读取所需数据
                    if (!read_iq_data_range_static(file_i, search_start + position, WINDOW_250US, i_buffer) ||
                        !read_iq_data_range_static(file_q, search_start + position, WINDOW_250US, q_buffer)) {
                        printf("Failed to read IQ data at position %d\n", search_start + position);
                        free(i_buffer);
                        free(q_buffer);
                        free(freq_data);
                        return result;
                    }
                    
                    // 解调计算频率
                    fm_demodulate_fx(i_buffer, q_buffer, WINDOW_250US, freq_data, SAMPLE_RATE);
                    
                    // 计算频率均值
                    short avg_freq = calc_freq_avg(freq_data, WINDOW_250US);
                    
                    if (is_freq_match(avg_freq, FREQ_1500)) {
                        consecutive_count++;
                        
                        // 需要至少17个连续0.25ms窗口 (4.5ms) 的1500Hz
                        if (consecutive_count == 1) {
                            first_1500_start = position;
                            //printf("Potential first 1200Hz found at %d (%.3fs)\n", 
                            //      position + search_start, (position + search_start)/(float)SAMPLE_RATE);
                        } else if (consecutive_count >= 15) {
                            first_1500_end = position + WINDOW_250US;
                            state = PARITY_STATE_FOUND_1500;
                            // printf("First 1200Hz signal confirmed: start=%d, end=%d (%.3f-%.3fs)\n",
                            //       first_1500_start + search_start, 
                            //       first_1500_end + search_start,
                            //       (first_1500_start + search_start)/(float)SAMPLE_RATE,
                            //       (first_1500_end + search_start)/(float)SAMPLE_RATE);
                            consecutive_count = 0;
                            result.is_odd = 0; // 偶数行
                            
                            // 释放当前状态的内存
                            free(i_buffer);
                            free(q_buffer);
                            free(freq_data);
                            continue;
                        }
                    }else if (is_freq_match(avg_freq, FREQ_2300)) {
                        consecutive_count++;
                        
                        // 需要至少17个连续0.25ms窗口 (9ms) 的2300Hz
                        if (consecutive_count == 1) {
                            first_2300_start = position;
                            //printf("Potential first 1200Hz found at %d (%.3fs)\n", 
                            //      position + search_start, (position + search_start)/(float)SAMPLE_RATE);
                        } else if (consecutive_count >= 15) {
                            first_2300_end = position + WINDOW_250US;
                            state = PARITY_STATE_FOUND_2300;
                            // printf("First 1200Hz signal confirmed: start=%d, end=%d (%.3f-%.3fs)\n",
                            //       first_1500_start + search_start, 
                            //       first_1500_end + search_start,
                            //       (first_1500_start + search_start)/(float)SAMPLE_RATE,
                            //       (first_1500_end + search_start)/(float)SAMPLE_RATE);
                            consecutive_count = 0;
                            result.is_odd = 1; // 奇数行
                            
                            // 释放当前状态的内存
                            free(i_buffer);
                            free(q_buffer);
                            free(freq_data);
                            continue;
                        }
                    }
                    else {
                        // 重置连续计数
                        consecutive_count = 0;
                        first_1500_start = -1;
                        first_2300_start = -1;
                    }
                    
                    position += STEP_250US;
                    
                    // 释放当前状态的内存
                    free(i_buffer);
                    free(q_buffer);
                    free(freq_data);
                }
                break;
                
            case PARITY_STATE_FOUND_1500:
                // 切换到使用0.25ms窗口搜索1900Hz信号
                state = PARITY_STATE_SEARCHING_1900;
                //printf("Searching for 1500Hz signal near %.3fs\n", 
                //      (position + search_start)/(float)SAMPLE_RATE);
                break;

            case PARITY_STATE_FOUND_2300:
                // 切换到使用0.25ms窗口搜索1900Hz信号
                state = PARITY_STATE_SEARCHING_1900;
                //printf("Searching for 1500Hz signal near %.3fs\n", 
                //      (position + search_start)/(float)SAMPLE_RATE);
                break;
                
            case PARITY_STATE_SEARCHING_1900:
                // 使用0.25ms窗口搜索1900Hz信号
                {
                    // 为当前状态分配内存
                    short* i_buffer = (short*)malloc(WINDOW_250US * sizeof(short));
                    short* q_buffer = (short*)malloc(WINDOW_250US * sizeof(short));
                    short* freq_data = (short*)malloc(WINDOW_250US * sizeof(short));
                    
                    if (!i_buffer || !q_buffer || !freq_data) {
                        printf("Memory allocation failed in STATE_SEARCHING_1500\n");
                        if (i_buffer) free(i_buffer);
                        if (q_buffer) free(q_buffer);
                        if (freq_data) free(freq_data);
                        return result;
                    }
                    
                    // 读取所需数据
                    if (!read_iq_data_range_static(file_i, search_start + position, WINDOW_250US, i_buffer) ||
                        !read_iq_data_range_static(file_q, search_start + position, WINDOW_250US, q_buffer)) {
                        printf("Failed to read IQ data at position %d\n", search_start + position);
                        free(i_buffer);
                        free(q_buffer);
                        free(freq_data);
                        return result;
                    }
                    
                    // 解调计算频率
                    fm_demodulate_fx(i_buffer, q_buffer, WINDOW_250US, freq_data, SAMPLE_RATE);
                    
                    // 计算频率均值
                    short avg_freq = calc_freq_avg(freq_data, WINDOW_250US);
                    
                    if (is_freq_match(avg_freq, FREQ_1900)) {
                        consecutive_count++;
                        
                        // 需要约6个连续的0.25ms窗口 (1.5ms) 的1900Hz
                        if (consecutive_count == 1) {
                            freq_1900_pos = position;
                            //printf("Potential 1500Hz found at %d (%.3fs)\n", 
                            //      position + search_start, (position + search_start)/(float)SAMPLE_RATE);
                        } else if (consecutive_count >= 4) {
                            freq_1900_end = position + WINDOW_250US;
                            //printf("1500Hz signal confirmed at position %d (%.3fs)\n", 
                            //      freq_1500_pos + search_start, (freq_1500_pos + search_start)/(float)SAMPLE_RATE);
                            
                            // 设置检测结果
                            result.found = 1;
                            result.sync_position = freq_1900_pos + search_start;
                            
                            // 设置检测完成标志
                            detection_completed = 1;
                            
                            // 释放当前状态的内存
                            free(i_buffer);
                            free(q_buffer);
                            free(freq_data);
                            continue;
                        }
                    } else {
                        // 非1500Hz，重置
                        consecutive_count = 0;
                    }
                    
                    position += STEP_250US;
                    
                    // 搜索限制，避免过远搜索
                    if (first_1500_end > -1 && position > first_1500_end + SAMPLE_RATE * 0.005) { // 最多搜索5ms
                        //printf("Could not find 1500Hz signal within range, resetting\n");
                        state = PARITY_STATE_IDLE;
                        position = first_1500_start + STEP_250US; // 回到初始1200Hz之后一点继续搜索
                        consecutive_count = 0;
                    }else if(first_2300_end > -1 && position > first_2300_end + SAMPLE_RATE * 0.005){
                        state = PARITY_STATE_IDLE;
                        position = first_2300_start + STEP_250US; // 回到初始1200Hz之后一点继续搜索
                        consecutive_count = 0;
                    }
                    
                    // 释放当前状态的内存
                    free(i_buffer);
                    free(q_buffer);
                    free(freq_data);
                }
                break;
                
            case PARITY_STATE_COMPLETE:
                // 已完成，直接设置标志
                detection_completed = 1;
                break;
        }
    }
    
    // 输出结果
    if (result.found) {
        printf("\nLine parity detected successfully:\n");
        printf("\nParity detection result: %s\n", result.is_odd ? "ODD" : "EVEN");
        printf("- Sync position (1900Hz): %d (%.3fs)\n", result.sync_position, result.sync_position/(float)SAMPLE_RATE);
    } else {
        printf("\nNo complete line parity found\n");
    }
    
    return result;
}

// // 奇偶行检测函数 - 在行同步检测完成后调用
// parity_result detect_line_parity(FILE* file_i, FILE* file_q, int search_start_point, int search_length_point) {
//     parity_result result = {0, 0, -1, 0};
    
    
    
//     // 计算分割脉冲起始位置 (行同步位置 + Y扫描时间)
//     int pulse_start = search_start_point;
    
//     // 分配内存
//     short* i_buffer = (short*)malloc(search_length_point * sizeof(short));
//     short* q_buffer = (short*)malloc(search_length_point * sizeof(short));
//     short* freq_data = (short*)malloc(search_length_point * sizeof(short));
    
//     if (!i_buffer || !q_buffer || !freq_data) {
//         printf("Memory allocation failed in detect_line_parity\n");
//         if (i_buffer) free(i_buffer);
//         if (q_buffer) free(q_buffer);
//         if (freq_data) free(freq_data);
//         return result;
//     }
    
//     // 读取分割脉冲IQ数据
//     if (!read_iq_data_range_static(file_i, pulse_start, search_length_point, i_buffer) ||
//         !read_iq_data_range_static(file_q, pulse_start, search_length_point, q_buffer)) {
//         printf("Failed to read pulse IQ data at position %d\n", pulse_start);
//         free(i_buffer);
//         free(q_buffer);
//         free(freq_data);
//         return result;
//     }
    
//     // 解调计算频率
//     fm_demodulate_fx(i_buffer, q_buffer, search_length_point, freq_data, SAMPLE_RATE);
    
//     // 计算频率均值
//     long freq_sum = 0;
//     for (int i = 0; i < search_length_point; i++) {
//         freq_sum += freq_data[i];
//     }
//     short avg_freq = (short)(freq_sum / search_length_point);
    
//     // 判断奇偶性 - 分割脉冲频率：偶数行≈1500Hz，奇数行≈2300Hz
//     int is_odd = abs(avg_freq - FREQ_2300) < abs(avg_freq - FREQ_1500);
    
//     // 计算门廊信号开始位置
//     int porch_start = pulse_start + search_length_point;
    
//     // 验证门廊信号是否真的是1900Hz (可选)
//     // 这里可以添加代码检查门廊信号的频率，但简化起见，我们假设它总是紧跟在分割脉冲之后
    
//     // 设置检测结果
//     result.found = 1;
//     result.is_odd = is_odd;
//     result.sync_position = porch_start;  // 设置同步点为门廊信号起始点
//     result.pulse_frequency = avg_freq;
    
//     // 释放内存
//     free(i_buffer);
//     free(q_buffer);
//     free(freq_data);
    
//     // 输出信息
//     printf("Line parity detection result:\n");
//     printf("- Line type: %s\n", result.is_odd ? "ODD" : "EVEN");
//     printf("- Pulse frequency: %d Hz\n", result.pulse_frequency);
//     printf("- Porch (sync) position: %d (%.3fs)\n", 
//            result.sync_position, 
//            result.sync_position/(float)SAMPLE_RATE);
    
//     return result;
// }

// 使用封装函数，简化调用
parity_result detect_line_parity_use(FILE* file_i, FILE* file_q, int search_start_point, int search_length_point) {
    
    parity_result result = detect_line_parity(file_i, file_q, search_start_point, search_length_point);

    if (result.found) {
        printf("\nSync position (1900Hz location) can be used as reference: %d\n", result.sync_position);
    }
    return result;
}