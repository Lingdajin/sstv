// #include <math.h>
// #include <stdio.h>
// #include <stdlib.h>
// #include "sbdsp/sbfft.h"

// #define SAMPLE_RATE 48000 // 采样率为48kHz
// #define DOWN_SAMPLE_RATE 8000    // 下采样率
// #define DOWN_NUM SAMPLE_RATE / DOWN_SAMPLE_RATE // 下采样间隔采样点数
// #define FFT_SIZE 256          // FFT大小

// void Asm_Mag(short *x,int n) 
// {
//     int Mag;
//     for(int i=0; i<n; i++)
//     {
//         Mag = sqrt(x[2 * i]*x[2 * i] + x[2 * i + 1]*x[2 * i + 1])/n;
//         printf("point %d is %d hz amplitude:%d \n",i,i * DOWN_SAMPLE_RATE / n,Mag);
//     }
// }

// int read_iq_data_range_static_BYTE(FILE* file, int offset, int count, signed char* buffer) {
//     if (file == NULL) {
//         printf("文件指针为空\n");
//         return 0;
//     }

//     long point = (long)(offset * sizeof(signed char));
//     //long point = (long)0;
//     // 定位到文件中的指定位置
//     if (fseek(file, point, SEEK_SET) != 0) {
//         printf("文件定位失败，偏移量: %ld\n", offset);
//         return 0;
//     }

//     // 读取指定数量的数据到提供的缓冲区
//     size_t read_count = fread(buffer, sizeof(signed char), count, file);
//     //*(buffer) = (*(buffer) & 0x00FF) << 8 | (*(buffer) & 0xFF00) >> 8;
//     if (read_count != count) {
//         printf("读取数据失败: 预期 %d 项，实际读取 %zu 项\n", count, read_count);
//         return 0;
//     }

//     return 1;
// }

// int main(){
//     const char* iq_data_i_path = "./sstv_data_file/sstv_iq_i_scaled_big_endian.bin"; // I数据文件路径
//     const char* iq_data_q_path = "./sstv_data_file/sstv_iq_q_scaled_big_endian.bin"; // Q数据文件路径
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

//     if (!read_iq_data_range_static_BYTE(file_i, 0, FFT_SIZE * DOWN_NUM, I_buffer)) {
//         printf("读取I数据失败\n");
//         fclose(file_i);
//         fclose(file_q);
//         return 1;
//     }
//     if (!read_iq_data_range_static_BYTE(file_q, 0, FFT_SIZE * DOWN_NUM, Q_buffer)) {
//         printf("读取Q数据失败\n");
//         fclose(file_i);
//         fclose(file_q);
//         return 1;
//     }

//     for(int i = 0; i < FFT_SIZE; i++){
//         fft_data[2 * i] = (short)I_buffer[DOWN_NUM * i];
//         fft_data[2 * i + 1] = (short)Q_buffer[DOWN_NUM * i];
//         printf("I:%d Q:%d \r\n", fft_data[2 * i], fft_data[2 * i + 1]);
//     }

//     sbfft256(fft_data, fft_out);

//     for(int i = 0; i < FFT_SIZE * 2; i++){
//         printf("fft_out: %d \r\n", fft_out[i]);
//     }

//     Asm_Mag(fft_out, FFT_SIZE); //幅度测量

// }
