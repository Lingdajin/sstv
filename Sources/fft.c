#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include "sbdsp/sbfft.h"

#define SAMPLE_RATE 48000 // 采样率为48kHz
#define DOWN_SAMPLE_RATE 8000    // 下采样率
#define DOWN_NUM SAMPLE_RATE / DOWN_SAMPLE_RATE // 下采样间隔采样点数
#define FFT_SIZE 256          // FFT大小

void Asm_Mag(short *x,int n) 
{
    int Mag;
    for(int i=0; i<n/2; i++)
    {
        Mag = (sqrt(x[2 * i]*x[2 * i] + x[2 * i + 1]*x[2 * i + 1]) + sqrt(x[(2 * n) - 1 - (2 * i)]*x[(2 * n) - 1 - (2 * i)] + x[(2 * n) - 2 - (2 * i)]*x[(2 * n) - 2 - (2 * i)]))/n;
        printf("point %d is %d hz amplitude:%d \n",i,i * DOWN_SAMPLE_RATE / n,Mag);
    }
}

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

// int main(){
//     const char* iq_data_i_path = "./sstv_data_file/iq_i_signal_with_noise_big_endian.bin"; // I数据文件路径
//     const char* iq_data_q_path = "./sstv_data_file/iq_q_signal_with_noise_big_endian.bin"; // Q数据文件路径
//     FILE* file_i = fopen(iq_data_i_path, "rb");
//     FILE* file_q = fopen(iq_data_q_path, "rb");

//     signed char I_buffer[FFT_SIZE * DOWN_NUM];
//     signed char Q_buffer[FFT_SIZE * DOWN_NUM];
//     short fft_data[FFT_SIZE * 2]; // FFT数据缓冲区，大小为FFT_SIZE * 2,实数部分和虚数部分交替存储
//     short fft_out[FFT_SIZE * 2]; // FFT输出缓冲区，大小为FFT_SIZE * 2,实数部分和虚数部分交替存储
//     if (file_i == NULL || file_q == NULL) {
//         printf("无法打开IQ数据文件\n");
//         if (file_i) fclose(file_i);
//         if (file_q) fclose(file_q);
//         return 1;
//     }

//     if (!read_iq_data_range_static_BYTE(file_i, 240000, FFT_SIZE * DOWN_NUM, I_buffer)) {
//         printf("读取I数据失败\n");
//         fclose(file_i);
//         fclose(file_q);
//         return 1;
//     }
//     if (!read_iq_data_range_static_BYTE(file_q, 240000, FFT_SIZE * DOWN_NUM, Q_buffer)) {
//         printf("读取Q数据失败\n");
//         fclose(file_i);
//         fclose(file_q);
//         return 1;
//     }

//     for(int i = 0; i < FFT_SIZE; i++){
//         fft_data[2 * i] = (short)I_buffer[DOWN_NUM * i];
//         fft_data[2 * i + 1] = (short)Q_buffer[DOWN_NUM * i];
//         // printf("I: %dQ: %d\r\n", fft_data[2 * i], fft_data[2 * i + 1]);
//         printf("%d\r\n", fft_data[2 * i + 1]);

//     }

//     sbfft256(fft_data, fft_out);

//     for(int i = 0; i < FFT_SIZE * 2; i++){
//         printf("fft_out: %d \r\n", fft_out[i]);
//     }

//     Asm_Mag(fft_out, FFT_SIZE); //幅度测量

// }
