#ifndef DETECT_PARITY_SYNC_H
#define DETECT_PARITY_SYNC_H

#include "user_task.h"


// 奇偶行检测相关常量
#define SAMPLE_RATE 48000       // 原始采样率
#define Y_SCAN_DURATION 0.088   // Y扫描持续时间(秒)
#define Y_SCAN_SAMPLES (int)(Y_SCAN_DURATION * SAMPLE_RATE)  // Y扫描样本数(约4224)
#define PULSE_DURATION 0.0045   // 分割脉冲持续时间(秒)
#define PULSE_SAMPLES (int)(PULSE_DURATION * SAMPLE_RATE)    // 分割脉冲样本数(约216)
#define PORCH_DURATION 0.0015   // 门廊信号持续时间(秒)
#define PORCH_SAMPLES (int)(PORCH_DURATION * SAMPLE_RATE)    // 门廊样本数(约72)
#define FREQ_1900 1900          // 门廊信号频率(Hz)
#define FREQ_2300 2300          // 奇数行分割脉冲频率(Hz)
#define FREQ_1500 1500          // 偶数行分割脉冲频率(Hz)



parity_result detect_line_parity(FILE* file_i, FILE* file_q, int search_start_point, int search_length_point);
parity_result detect_line_parity_use(FILE* file_i, FILE* file_q, int search_start_point, int search_length_point);

#endif // DETECT_PARITY_SYNC_H
