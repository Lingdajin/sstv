#include "detect_vis_sync.h"



// 计算频率均值
short calc_freq_avg(short* freq_data, int length) {
    long sum = 0;
    for (int i = 1; i < length - 1; i++) {
        sum += freq_data[i];
    }
    return (short)(sum / (length - 2));
}

// 检查频率是否在目标频率容差范围内
int is_freq_match(short freq, short target_freq) {
    return abs(freq - target_freq) <= FREQ_TOLERANCE;
}

// 状态机检测VIS同步头 - 每个状态单独管理内存
vis_result detect_vis_sync(FILE* file_i, FILE* file_q, int search_start, int search_length) {
    vis_result result = {0, -1, -1, -1};
    
    // 状态机变量
    VIS_State state = VIS_STATE_IDLE;
    int position = 0;
    int first_1900_start = -1;
    int first_1900_end = -1;
    int freq_1200_pos = -1;
    int second_1900_start = -1;
    int consecutive_count = 0;  // 连续匹配计数
    int detection_completed = 0;  // 检测完成标志
    
    printf("Starting VIS sync detection...\n");
    
    // 循环处理信号，每个状态都有自己的缓冲区
    while (position + WINDOW_10MS < search_length && !detection_completed) {
        switch (state) {
            case VIS_STATE_IDLE:
                // 使用10ms窗口和步长搜索第一个1900Hz信号
                {
                    // 为当前状态分配内存
                    short* i_buffer = (short*)malloc(WINDOW_10MS * sizeof(short));
                    short* q_buffer = (short*)malloc(WINDOW_10MS * sizeof(short));
                    short* freq_data = (short*)malloc(WINDOW_10MS * sizeof(short));
                    
                    if (!i_buffer || !q_buffer || !freq_data) {
                        printf("Memory allocation failed in STATE_IDLE\n");
                        if (i_buffer) free(i_buffer);
                        if (q_buffer) free(q_buffer);
                        if (freq_data) free(freq_data);
                        return result;
                    }
                    
                    // 读取所需数据
                    if (!read_iq_data_range_static(file_i, search_start + position, WINDOW_10MS, i_buffer) ||
                        !read_iq_data_range_static(file_q, search_start + position, WINDOW_10MS, q_buffer)) {
                        printf("Failed to read IQ data at position %d\n", search_start + position);
                        free(i_buffer);
                        free(q_buffer);
                        free(freq_data);
                        return result;
                    }
                    
                    // 解调计算频率
                    fm_demodulate_fx(i_buffer, q_buffer, WINDOW_10MS, freq_data, SAMPLE_RATE);
                    
                    // 计算频率均值
                    short avg_freq = calc_freq_avg(freq_data, WINDOW_10MS);
                    
                    if (is_freq_match(avg_freq, FREQ_1900)) {
                        consecutive_count++;
                        
                        // 需要至少30个连续10ms窗口 (300ms) 的1900Hz
                        if (consecutive_count == 1) {
                            first_1900_start = position;
                            printf("Potential first 1900Hz found at %d (%.3fs)\n", 
                                  position + search_start, (position + search_start)/(float)SAMPLE_RATE);
                        } else if (consecutive_count >= 28) {
                            first_1900_end = position + WINDOW_10MS;
                            state = VIS_STATE_FOUND_1900_FIRST;
                            printf("First 1900Hz signal confirmed: start=%d, end=%d (%.3f-%.3fs)\n",
                                  first_1900_start + search_start, 
                                  first_1900_end + search_start,
                                  (first_1900_start + search_start)/(float)SAMPLE_RATE,
                                  (first_1900_end + search_start)/(float)SAMPLE_RATE);
                            position = first_1900_end;  // 切换到下一个状态
                            consecutive_count = 0;
                            
                            // 释放当前状态的内存
                            free(i_buffer);
                            free(q_buffer);
                            free(freq_data);
                            continue;
                        }
                    } else {
                        // 重置连续计数
                        consecutive_count = 0;
                        first_1900_start = -1;
                    }
                    
                    position += STEP_10MS;
                    
                    // 释放当前状态的内存
                    free(i_buffer);
                    free(q_buffer);
                    free(freq_data);
                }
                break;
                
            case VIS_STATE_FOUND_1900_FIRST:
                // 切换到使用1ms窗口搜索1200Hz信号
                state = VIS_STATE_SEARCHING_1200;
                printf("Searching for 1200Hz signal near %.3fs\n", 
                      (position + search_start)/(float)SAMPLE_RATE);
                break;
                
            case VIS_STATE_SEARCHING_1200:
                // 使用1ms窗口搜索1200Hz信号
                {
                    // 为当前状态分配内存
                    short* i_buffer = (short*)malloc(WINDOW_1MS * sizeof(short));
                    short* q_buffer = (short*)malloc(WINDOW_1MS * sizeof(short));
                    short* freq_data = (short*)malloc(WINDOW_1MS * sizeof(short));
                    
                    if (!i_buffer || !q_buffer || !freq_data) {
                        printf("Memory allocation failed in STATE_SEARCHING_1200\n");
                        if (i_buffer) free(i_buffer);
                        if (q_buffer) free(q_buffer);
                        if (freq_data) free(freq_data);
                        return result;
                    }
                    
                    // 读取所需数据
                    if (!read_iq_data_range_static(file_i, search_start + position, WINDOW_1MS, i_buffer) ||
                        !read_iq_data_range_static(file_q, search_start + position, WINDOW_1MS, q_buffer)) {
                        printf("Failed to read IQ data at position %d\n", search_start + position);
                        free(i_buffer);
                        free(q_buffer);
                        free(freq_data);
                        return result;
                    }
                    
                    // 解调计算频率
                    fm_demodulate_fx(i_buffer, q_buffer, WINDOW_1MS, freq_data, SAMPLE_RATE);
                    
                    // 计算频率均值
                    short avg_freq = calc_freq_avg(freq_data, WINDOW_1MS);
                    
                    if (is_freq_match(avg_freq, FREQ_1200)) {
                        consecutive_count++;
                        
                        // 需要约10个连续的1ms窗口 (10ms) 的1200Hz
                        if (consecutive_count == 1) {
                            freq_1200_pos = position;
                            printf("Potential 1200Hz found at %d (%.3fs)\n", 
                                  position + search_start, (position + search_start)/(float)SAMPLE_RATE);
                        } else if (consecutive_count >= 9) {
                            state = VIS_STATE_FOUND_1200;
                            printf("1200Hz signal confirmed at position %d (%.3fs)\n", 
                                  freq_1200_pos + search_start, (freq_1200_pos + search_start)/(float)SAMPLE_RATE);
                            position = position + WINDOW_1MS;
                            consecutive_count = 0;
                            
                            // 释放当前状态的内存
                            free(i_buffer);
                            free(q_buffer);
                            free(freq_data);
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
                        state = VIS_STATE_IDLE;
                        position = first_1900_start + STEP_10MS; // 回到初始1900Hz之后一点继续搜索
                        consecutive_count = 0;
                    }
                    
                    // 释放当前状态的内存
                    free(i_buffer);
                    free(q_buffer);
                    free(freq_data);
                }
                break;
                
            case VIS_STATE_FOUND_1200:
                // 搜索第二个1900Hz信号
                {
                    // 为当前状态分配内存
                    short* i_buffer = (short*)malloc(WINDOW_10MS * sizeof(short));
                    short* q_buffer = (short*)malloc(WINDOW_10MS * sizeof(short));
                    short* freq_data = (short*)malloc(WINDOW_10MS * sizeof(short));
                    
                    if (!i_buffer || !q_buffer || !freq_data) {
                        printf("Memory allocation failed in STATE_FOUND_1200\n");
                        if (i_buffer) free(i_buffer);
                        if (q_buffer) free(q_buffer);
                        if (freq_data) free(freq_data);
                        return result;
                    }
                    
                    // 读取所需数据
                    if (!read_iq_data_range_static(file_i, search_start + position, WINDOW_10MS, i_buffer) ||
                        !read_iq_data_range_static(file_q, search_start + position, WINDOW_10MS, q_buffer)) {
                        printf("Failed to read IQ data at position %d\n", search_start + position);
                        free(i_buffer);
                        free(q_buffer);
                        free(freq_data);
                        return result;
                    }
                    
                    // 解调计算频率
                    fm_demodulate_fx(i_buffer, q_buffer, WINDOW_10MS, freq_data, SAMPLE_RATE);
                    
                    // 计算频率均值
                    short avg_freq = calc_freq_avg(freq_data, WINDOW_10MS);
                    
                    if (is_freq_match(avg_freq, FREQ_1900)) {
                        consecutive_count++;
                        
                        // 标记第二个1900Hz开始位置
                        if (consecutive_count == 1) {
                            second_1900_start = position;
                            printf("Potential second 1900Hz found at %d (%.3fs)\n", 
                                  position + search_start, (position + search_start)/(float)SAMPLE_RATE);
                        } else if (consecutive_count >= 29) { // 需要至少300ms的1900Hz
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
                            
                            // 设置检测完成标志
                            detection_completed = 1;
                            
                            // 释放当前状态的内存
                            free(i_buffer);
                            free(q_buffer);
                            free(freq_data);
                            continue;
                        }
                    } else {
                        // 非1900Hz，重置计数
                        consecutive_count = 0;
                    }
                    
                    position += STEP_10MS;
                    
                    // 搜索限制，避免过远搜索
                    if (position > freq_1200_pos + SAMPLE_RATE * 0.5) {
                        printf("Could not find second 1900Hz signal within range, resetting\n");
                        state = VIS_STATE_IDLE;
                        position = first_1900_start + STEP_10MS;
                        consecutive_count = 0;
                    }
                    
                    // 释放当前状态的内存
                    free(i_buffer);
                    free(q_buffer);
                    free(freq_data);
                }
                break;
                
            case VIS_STATE_COMPLETE:
                // 已完成，直接设置标志
                detection_completed = 1;
                break;
        }
    }
    
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

//主函数
vis_result detect_vis_sync_use(FILE* file_i, FILE* file_q, int search_start, int search_length) {
    // const char* iq_data_i_path = "./sstv_data_file/sstv_iq_i_big_endian.bin";
    // const char* iq_data_q_path = "./sstv_data_file/sstv_iq_q_big_endian.bin";
    
    // FILE* file_i = fopen(iq_data_i_path, "rb");
    // FILE* file_q = fopen(iq_data_q_path, "rb");
    
    // if (file_i == NULL || file_q == NULL) {
    //     printf("Could not open IQ data files\n");
    //     if (file_i) fclose(file_i);
    //     if (file_q) fclose(file_q);
    //     return 1;
    // }
    
    //搜索参数
    int search_start_point = SAMPLE_RATE * search_start;
    int search_length_point = SAMPLE_RATE * search_length;  // 搜索
    
    vis_result result = detect_vis_sync(file_i, file_q, search_start_point, search_length_point);
    
    if (result.found) {
        printf("\nSync position (1200Hz location) can be used as reference: %d\n", result.sync_position);
    }
    
    return result;
}