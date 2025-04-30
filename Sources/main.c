//#include "sstv_iq_fs48k.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include "sbdsp/sbatan.h"

// 定点数定义
#define FX_SCALE 1024        // 使用Q10格式 (2^10)
#define FX_SCALE_LOG2 10     // 2^10 = 1024
#define FX_PI 3217           // PI * FX_SCALE
#define FX_DOUBLE(x) ((int)((x) * FX_SCALE + 0.5))  // 浮点数转定点数
#define TO_DOUBLE(x) ((double)(x) / FX_SCALE)       // 定点数转浮点数

// sb_fxatan常量定义
#define SB_FXATAN_MAX 32767   // sb_fxatan输出对应π的最大值
#define SB_FXATAN_PI_SCALE 32767  // sb_fxatan中π对应的比例因子

// 定点数版的常量
#define FX_SYNC_FREQ FX_DOUBLE(1200.0)    // 1200.0 * FX_SCALE
#define FX_BLACK_FREQ FX_DOUBLE(1500.0)   // 1500.0 * FX_SCALE
#define FX_WHITE_FREQ FX_DOUBLE(2300.0)   // 2300.0 * FX_SCALE
#define FX_FREQ_RANGE (FX_WHITE_FREQ - FX_BLACK_FREQ)  // 范围

// 原始浮点常量保留，确保代码兼容性
#define PI 3.14159265358979323846
#define SAMPLE_RATE 48000 // 采样率为48kHz

// SSTV扫描参数
#define Y_NUM 4224        // Y信号采样点数 (88ms * 48kHz)
#define Y_BEGINE 44256    // Y信号开始采样点
#define Y_INTERVAL 7200   // Y信号间隔

#define RY_NUM 2112       // RY信号采样点数 (44ms * 48kHz)
#define RY_BEGINE 48768   // RY信号开始采样点
#define RY_INTERVAL 14400 // RY信号间隔

#define BY_NUM 2112       // BY信号采样点数 (44ms * 48kHz)
#define BY_BEGINE 55968   // BY信号开始采样点
#define BY_INTERVAL 14400 // BY信号间隔

// SSTV频率范围定义
#define SYNC_FREQ 1200
#define BLACK_FREQ 1500
#define WHITE_FREQ 2300
#define FREQ_RANGE (WHITE_FREQ - BLACK_FREQ)

// 图像定义
#define IMAGE_WIDTH 320   // 图像宽度(像素)
#define IMAGE_HEIGHT 240  // 图像高度(行数)
#define TOTAL_SAMPLES 1771680 // 总采样点数

// BMP文件相关定义
#define BYTES_PER_PIXEL 3 // RGB每像素3字节
#define FILE_HEADER_SIZE 14
#define INFO_HEADER_SIZE 40
#define PADDING_SIZE(width) ((4 - ((width) * BYTES_PER_PIXEL) % 4) % 4)

// 函数声明
void fm_demodulate_fx(short* I, short* Q, int length, short* freq_fx, int sample_rate);
void freq_to_yuv_fx(short* freq_fx, int length, short* Y_fx);
void map_to_pixels_fx(short* data_fx, int data_length, short* pixels_fx, int pixel_count);
void yuv_to_rgb(short Y, short RY, short BY, short* R, short* G, short* B);
void save_bmp(const char* filename, unsigned char* image_data, int width, int height);

// 新增函数声明
void extract_scan_line_fx(FILE* file_i, FILE* file_q, int start_sample, int num_samples, short* pixels_fx, int pixel_count);
void process_line_y_fx(FILE* file_i, FILE* file_q, int line_number, short* y_line_fx);
void process_line_ry_by_fx(FILE* file_i, FILE* file_q, int group_number, short* ry_line_fx, short* by_line_fx);

// 修改函数声明，用于读取指定范围的IQ数据
int read_iq_data_range_static(FILE* file, int offset, int count, short* buffer);

// 全局数据数组 - 改为定点数形式
short fx_y_lines[IMAGE_HEIGHT][IMAGE_WIDTH];      // 存储所有行的Y值(定点数)
short fx_ry_lines[IMAGE_HEIGHT/2][IMAGE_WIDTH];   // 存储RY值(定点数)
short fx_by_lines[IMAGE_HEIGHT/2][IMAGE_WIDTH];   // 存储BY值(定点数)

int main() {
    const char* output_file_path = "./output/sstv_decoded.txt";
    const char* bmp_file_path = "./output/sstv_image.bmp";
    const char* iq_data_i_path = "./sstv_data_file/sstv_iq_i_big_endian.bin"; // I数据文件路径
    const char* iq_data_q_path = "./sstv_data_file/sstv_iq_q_big_endian.bin"; // Q数据文件路径

    // 打开IQ数据文件，保持文件指针打开
    FILE* file_i = fopen(iq_data_i_path, "rb");
    FILE* file_q = fopen(iq_data_q_path, "rb");

    if (file_i == NULL || file_q == NULL) {
        printf("无法打开IQ数据文件\n");
        if (file_i) fclose(file_i);
        if (file_q) fclose(file_q);
        return 1;
    }

    FILE *output_file = fopen(output_file_path, "w");
    if (output_file == NULL) {
        printf("无法打开输出文件,请自行创建/output文件夹以存放输出文件\n");
        fclose(file_i);
        fclose(file_q);
        return 1;
    }

    printf("开始SSTV解码...\n");

    // 处理每一行Y值 - 使用定点数版本
    for(int line = 0; line < IMAGE_HEIGHT; line++) {
        process_line_y_fx(file_i, file_q, line, fx_y_lines[line]);
        printf("处理Y行: %d/240\r", line+1);
    }

    // 处理每两行的RY/BY值 - 使用定点数版本
    for(int group = 0; group < IMAGE_HEIGHT/2; group++) {
        process_line_ry_by_fx(file_i, file_q, group, fx_ry_lines[group], fx_by_lines[group]);
        printf("处理RY/BY组: %d/120\r", group+1);
    }

    // 输出解码结果到文件 - 添加RGB信息并优化格式
    // fprintf(output_file, "# SSTV解码数据分析报告\n");
    // fprintf(output_file, "# 图像尺寸: %d x %d 像素\n", IMAGE_WIDTH, IMAGE_HEIGHT);
    // fprintf(output_file, "# 解码时间: %s\n\n", __DATE__);

    // fprintf(output_file, "# 数据格式说明:\n");
    // fprintf(output_file, "# Y:    亮度信号 (0-255)\n");
    // fprintf(output_file, "# R-Y:  红色色度差信号\n");
    // fprintf(output_file, "# B-Y:  蓝色色度差信号\n");
    // fprintf(output_file, "# R,G,B: 转换后的RGB颜色值 (0-255)\n\n");

    // fprintf(output_file, "行号,像素,Y值,R-Y值,B-Y值,R值,G值,B值\n");
    // fprintf(output_file, "--------------------------------------\n");

//    // 统计信息变量
//    double total_brightness = 0.0;
//    int dark_pixels = 0;
//    int bright_pixels = 0;

//    for(int line = 0; line < IMAGE_HEIGHT; line++) {
//        int group = line / 2;
//
//        for(int pixel = 0; pixel < IMAGE_WIDTH; pixel++) {
//            unsigned char R, G, B;
//
//            // 使用定点数版YUV到RGB转换
//            yuv_to_rgb_fx(
//                fx_y_lines[line][pixel],
//                fx_ry_lines[group][pixel],
//                fx_by_lines[group][pixel],
//                &R, &G, &B
//            );
//
//            // 更新统计信息 - 需要转换回浮点数
//            total_brightness += TO_DOUBLE(fx_y_lines[line][pixel]);
//            if(TO_DOUBLE(fx_y_lines[line][pixel]) < 50) dark_pixels++;
//            if(TO_DOUBLE(fx_y_lines[line][pixel]) > 200) bright_pixels++;
//
//            // // 只输出每20个像素的数据，防止文件过大
//            // if(pixel % 20 == 0) {
//            //     fprintf(output_file, "%3d,%3d,%6.2f,%6.2f,%6.2f,%3d,%3d,%3d\n",
//            //             line, pixel,
//            //             y_lines[line][pixel],
//            //             ry_lines[group][pixel],
//            //             by_lines[group][pixel],
//            //             R, G, B);
//            // }
//            // printf("%3d,%3d,%d,%d,%d,%3d,%3d,%3d\n",
//            //                 line, pixel,
//            //                 fx_y_lines[line][pixel],
//            //                 fx_ry_lines[group][pixel],
//            //                 fx_by_lines[group][pixel],
//            //                 R, G, B);
//         }
//    }

    // 添加统计摘要
    // fprintf(output_file, "\n\n# 图像统计摘要\n");
    // fprintf(output_file, "平均亮度: %.2f\n", total_brightness / (IMAGE_WIDTH * IMAGE_HEIGHT));
    // fprintf(output_file, "暗像素数量(Y<50): %d (%.2f%%)\n",
    //         dark_pixels, (dark_pixels * 100.0) / (IMAGE_WIDTH * IMAGE_HEIGHT));
    // fprintf(output_file, "亮像素数量(Y>200): %d (%.2f%%)\n",
    //         bright_pixels, (bright_pixels * 100.0) / (IMAGE_WIDTH * IMAGE_HEIGHT));

    // 创建RGB图像
    unsigned char* image_data = (unsigned char*)malloc(IMAGE_WIDTH * IMAGE_HEIGHT * BYTES_PER_PIXEL);
    if (image_data == NULL) {
        printf("内存分配失败\n");
        fclose(output_file);
        return 1;
    }

    // 将YUV转换为RGB
    printf("\n将YUV转换为RGB...\n");
    for(int line = 0; line < IMAGE_HEIGHT; line++) {
        int group = line / 2;

        for(int pixel = 0; pixel < IMAGE_WIDTH; pixel++) {
            short R, G, B;

            // YUV到RGB转换
            yuv_to_rgb(
                fx_y_lines[line][pixel],
                fx_ry_lines[group][pixel],
                fx_by_lines[group][pixel],
                &R, &G, &B
            );

            // 在BMP图像中，像素按BGR顺序存储
            int idx = (line * IMAGE_WIDTH + pixel) * BYTES_PER_PIXEL;
            image_data[idx] = B;     // B
            image_data[idx + 1] = G; // G
            image_data[idx + 2] = R; // R
        }
    }

    // 保存为BMP文件
    save_bmp(bmp_file_path, image_data, IMAGE_WIDTH, IMAGE_HEIGHT);

    printf("解码完成!\n");
    printf("- 数据已保存到: %s\n", output_file_path);
    printf("- 图像已保存到: %s\n", bmp_file_path);

    // 释放资源
    free(image_data);
    fclose(file_i);
    fclose(file_q);
    fclose(output_file);

    return 0;
}

// 定点数版的YUV到RGB转换
void yuv_to_rgb(short Y, short RY, short BY, short* R, short* G, short* B) {
    // 标准YUV到RGB转换
	double r = 0.003906*((298.082*(Y - 16)) + (408.583*(RY - 128))); // YUV到RGB转换
	double g = 0.003906 * ((298.082 * (Y - 16.0)) + (-100.291 * (BY - 128.0)) + (-208.12 * (RY - 128.0)));
	double b = 0.003906 * ((298.082 * (Y - 16.0)) + (516.411 * (BY - 128.0)));

    // 限制在0-255范围内
	*R = (r < 0) ? 0 : ((r > 255) ? 255 : (short)r);
	*G = (g < 0) ? 0 : ((g > 255) ? 255 : (short)g);
	*B = (b < 0) ? 0 : ((b > 255) ? 255 : (short)b);

}

// 保存BMP文件
void save_bmp(const char* filename, unsigned char* image_data, int width, int height) {
    FILE* file = fopen(filename, "wb");
    if (file == NULL) {
        printf("无法创建BMP文件\n");
        return;
    }

    int padding_size = PADDING_SIZE(width);
    int stride = width * BYTES_PER_PIXEL + padding_size;
    int file_size = FILE_HEADER_SIZE + INFO_HEADER_SIZE + (stride * height);

    // BMP文件头
    unsigned char file_header[FILE_HEADER_SIZE] = {
        'B', 'M',                   // 文件类型标识
        file_size & 0xFF,           // 文件大小
        (file_size >> 8) & 0xFF,
        (file_size >> 16) & 0xFF,
        (file_size >> 24) & 0xFF,
        0, 0, 0, 0,                 // 保留
        FILE_HEADER_SIZE + INFO_HEADER_SIZE & 0xFF,  // 像素数据偏移
        ((FILE_HEADER_SIZE + INFO_HEADER_SIZE) >> 8) & 0xFF,
        ((FILE_HEADER_SIZE + INFO_HEADER_SIZE) >> 16) & 0xFF,
        ((FILE_HEADER_SIZE + INFO_HEADER_SIZE) >> 24) & 0xFF
    };

    // BMP信息头
    unsigned char info_header[INFO_HEADER_SIZE] = {
        INFO_HEADER_SIZE & 0xFF,             // 信息头大小
        (INFO_HEADER_SIZE >> 8) & 0xFF,
        (INFO_HEADER_SIZE >> 16) & 0xFF,
        (INFO_HEADER_SIZE >> 24) & 0xFF,
        width & 0xFF,                        // 宽度
        (width >> 8) & 0xFF,
        (width >> 16) & 0xFF,
        (width >> 24) & 0xFF,
        height & 0xFF,                       // 高度
        (height >> 8) & 0xFF,
        (height >> 16) & 0xFF,
        (height >> 24) & 0xFF,
        1, 0,                                // 颜色平面数
        BYTES_PER_PIXEL * 8, 0,              // 每像素位数
        0, 0, 0, 0,                          // 压缩方式(无压缩)
        0, 0, 0, 0,                          // 图像大小
        0, 0, 0, 0,                          // 水平分辨率
        0, 0, 0, 0,                          // 垂直分辨率
        0, 0, 0, 0,                          // 调色板颜色数
        0, 0, 0, 0                           // 重要颜色数
    };

    // 写入文件头和信息头
    fwrite(file_header, 1, FILE_HEADER_SIZE, file);
    fwrite(info_header, 1, INFO_HEADER_SIZE, file);

    // 准备行填充
    unsigned char padding[3] = {0, 0, 0};

    // BMP文件以从下到上的顺序存储行
    for (int y = height - 1; y >= 0; y--) {
        // 写入一行像素数据
        fwrite(&image_data[y * width * BYTES_PER_PIXEL], BYTES_PER_PIXEL, width, file);

        // 添加行填充(如果需要)
        if (padding_size > 0) {
            fwrite(padding, 1, padding_size, file);
        }
    }

    fclose(file);
}

// 使用定点数版本的扫描线提取函数
void extract_scan_line_fx(FILE* file_i, FILE* file_q, int start_sample, int num_samples, short* pixels_fx, int pixel_count) {
    // 使用静态缓冲区代替动态内存分配
    static short I_buffer[Y_NUM]; // 使用最大的Y_NUM作为缓冲区大小
    static short Q_buffer[Y_NUM];
    static short freq_fx[Y_NUM];
    static short color_fx[Y_NUM];
    static short dummy1_fx[Y_NUM];
    static short dummy2_fx[Y_NUM];

    if(num_samples > Y_NUM) {
        printf("扫描线样本数超出缓冲区大小\n");
        return;
    }

    // 读取IQ数据到静态缓冲区
    if(!read_iq_data_range_static(file_i, start_sample, num_samples, I_buffer) ||
       !read_iq_data_range_static(file_q, start_sample, num_samples, Q_buffer)) {
        printf("无法读取IQ数据\n");
        return;
    }

    // 使用定点数版FM解调
    fm_demodulate_fx(I_buffer, Q_buffer, num_samples, freq_fx, SAMPLE_RATE);

    // 使用定点数版频率转YUV
    freq_to_yuv_fx(freq_fx, num_samples - 1, color_fx);

    // 使用定点数版映射到像素
    map_to_pixels_fx(color_fx, num_samples - 1, pixels_fx, pixel_count);
}

// 处理单行Y值 - 定点数版本
void process_line_y_fx(FILE* file_i, FILE* file_q, int line_number, short* y_line_fx) {
    // 计算Y扫描线的起始位置
    int start_sample = Y_BEGINE + (line_number * Y_INTERVAL);

    // 确保不超出信号范围
    if(start_sample + Y_NUM > TOTAL_SAMPLES) {
        printf("警告：行 %d 的Y信号超出采样范围\n", line_number);
        // 填充零值
        for(int i = 0; i < IMAGE_WIDTH; i++) {
            y_line_fx[i] = 0;
        }
        return;
    }

    // 提取该行的Y值 - 使用定点数版本
    extract_scan_line_fx(file_i, file_q, start_sample, Y_NUM, y_line_fx, IMAGE_WIDTH);
}

// 处理RY和BY值(每两行共享一组) - 定点数版本
void process_line_ry_by_fx(FILE* file_i, FILE* file_q, int group_number, short* ry_line_fx, short* by_line_fx) {
    // 计算RY和BY扫描的起始位置
    int ry_start = RY_BEGINE + (group_number * RY_INTERVAL);
    int by_start = BY_BEGINE + (group_number * BY_INTERVAL);

    // 确保不超出信号范围
    if(ry_start + RY_NUM > TOTAL_SAMPLES) {
        printf("警告：组 %d 的RY信号超出采样范围\n", group_number);
        for(int i = 0; i < IMAGE_WIDTH; i++) {
            ry_line_fx[i] = 0;
        }
    } else {
        // 提取该组的RY值 - 使用定点数版本
        extract_scan_line_fx(file_i, file_q, ry_start, RY_NUM, ry_line_fx, IMAGE_WIDTH);
    }

    if(by_start + BY_NUM > TOTAL_SAMPLES) {
        printf("警告：组 %d 的BY信号超出采样范围\n", group_number);
        for(int i = 0; i < IMAGE_WIDTH; i++) {
            by_line_fx[i] = 0;
        }
    } else {
        // 提取该组的BY值 - 使用定点数版本
        extract_scan_line_fx(file_i, file_q, by_start, BY_NUM, by_line_fx, IMAGE_WIDTH);
    }
}

// 定点数版的FM解调函数
void fm_demodulate_fx(short* I, short* Q, int length, short* freq_fx, int sample_rate) {
    static short fx_phase[Y_NUM];       // 使用静态数组避免栈溢出
    static int fx_unwrapped[Y_NUM];


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
        if(diff > SB_FXATAN_MAX) diff -= 2 * SB_FXATAN_MAX;
        else if(diff < -SB_FXATAN_MAX) diff += 2 * SB_FXATAN_MAX;

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
        freq_fx[i] = (phase_diff * sample_rate) / (2 * SB_FXATAN_PI_SCALE);
        //printf("freq_fx[%d]: %d\n", i, freq_fx[i]);
    }
}

// 定点数版的频率转YUV函数
void freq_to_yuv_fx(short* freq_fx, int length, short* Y_fx) {


    for(int i = 0; i < length; i++) {
        // 根据SSTV标准, 将频率转换为亮度值
        if(freq_fx[i] <= BLACK_FREQ) {
            Y_fx[i] = 0;
        } else if(freq_fx[i] >= WHITE_FREQ) {
            Y_fx[i] = 255;
        } else {
            // Y = ((freq - BLACK_FREQ) / FREQ_RANGE) * 255.0
            Y_fx[i] = ((freq_fx[i] - BLACK_FREQ) * 255 / FREQ_RANGE);
        }

        // 计算RY和BY
        //R_Y_fx[i] = ((freq_fx[i] - BLACK_FREQ) / FREQ_RANGE) * 255 - 128;
        //B_Y_fx[i] = ((freq_fx[i] - BLACK_FREQ) / FREQ_RANGE) * 255 - 128;
    }
}

// 定点数版的数据映射到像素
void map_to_pixels_fx(short* data_fx, int data_length, short* pixels_fx, int pixel_count) {
    // 使用整数计算每个像素对应的采样点数
    //double samples_per_pixel_fx = (double)data_length / pixel_count;
	//由于samples_per_pixel_fx只可能取两个值，分别对应data_length为4224-1和2112-1，提前算出并赋值，以降低浮点运算资源占用
	double samples_per_pixel_fx;
    if(data_length == 4224 - 1){
    	samples_per_pixel_fx = 13.196875;
    }else if(data_length == 2112 - 1){
    	samples_per_pixel_fx = 6.596875;
    }
//	short samples_per_pixel_fx;
//	    if(data_length == 4224 - 1){
//	    	samples_per_pixel_fx = 13;
//	    }else if(data_length == 2112 - 1){
//	    	samples_per_pixel_fx = 6;
//	    }

    for(int pixel = 0; pixel < pixel_count; pixel++) {
        // 计算像素对应的采样点范围
        int start_idx = pixel * samples_per_pixel_fx;
        //int end_idx = ((pixel + 1) * (samples_per_pixel_fx - 1)) - 1;
        int end_idx = start_idx + samples_per_pixel_fx - 1;

        // 边界检查
        if(end_idx >= data_length) end_idx = data_length - 1;
        if(start_idx < 0) start_idx = 0;

        // 累加像素范围内的数据值
        int sum = 0;
        int count = 0;

            for(int i = start_idx; i <= end_idx; i++) {
                sum += data_fx[i];
                count++;
            }

        // 计算平均值
        pixels_fx[pixel] = (count > 0) ? (sum / count) : 0;
    }
}

// 优化的文件读取函数 - 使用静态缓冲区
int read_iq_data_range_static(FILE* file, int offset, int count, short* buffer) {
    if (file == NULL) {
        printf("文件指针为空\n");
        return 0;
    }

    long point = (long)(offset * sizeof(short));
    //long point = (long)0;
    // 定位到文件中的指定位置
    if (fseek(file, point, SEEK_SET) != 0) {
        printf("文件定位失败，偏移量: %ld\n", offset);
        return 0;
    }

    // 读取指定数量的数据到提供的缓冲区
    size_t read_count = fread(buffer, sizeof(short), count, file);
    //*(buffer) = (*(buffer) & 0x00FF) << 8 | (*(buffer) & 0xFF00) >> 8;
    if (read_count != count) {
        printf("读取数据失败: 预期 %d 项，实际读取 %zu 项\n", count, read_count);
        return 0;
    }

    return 1;
}
