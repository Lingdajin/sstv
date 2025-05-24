#ifndef DETECT_PARITY_SYNC_H
#define DETECT_PARITY_SYNC_H

#include "user_task.h"


// 定义常量
#define SAMPLE_RATE 48000       // 原始采样率
#define FREQ_1500 1500          // 偶数目标频率(Hz)
#define FREQ_2300 2300          // 奇数目标频率(Hz)
#define FREQ_1900 1900          // 门廊目标频率(Hz)
#define FREQ_TOLERANCE 50       // 频率容差(Hz)
#define WINDOW_250US 12           // 0.25ms窗口大小 (48000 * 0.00025)
#define STEP_250US 12             // 0.25ms步长



parity_result detect_line_parity(FILE* file_i, FILE* file_q, int search_start, int search_length);
parity_result detect_line_parity_use(FILE* file_i, FILE* file_q, int search_start_point, int search_length_point);

#endif // DETECT_PARITY_SYNC_H
