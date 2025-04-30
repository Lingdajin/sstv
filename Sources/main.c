//#include "sstv_iq_fs48k.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include "sbdsp/sbatan.h"

// ����������
#define FX_SCALE 1024        // ʹ��Q10��ʽ (2^10)
#define FX_SCALE_LOG2 10     // 2^10 = 1024
#define FX_PI 3217           // PI * FX_SCALE
#define FX_DOUBLE(x) ((int)((x) * FX_SCALE + 0.5))  // ������ת������
#define TO_DOUBLE(x) ((double)(x) / FX_SCALE)       // ������ת������

// sb_fxatan��������
#define SB_FXATAN_MAX 32767   // sb_fxatan�����Ӧ�е����ֵ
#define SB_FXATAN_PI_SCALE 32767  // sb_fxatan�Цж�Ӧ�ı�������

// ��������ĳ���
#define FX_SYNC_FREQ FX_DOUBLE(1200.0)    // 1200.0 * FX_SCALE
#define FX_BLACK_FREQ FX_DOUBLE(1500.0)   // 1500.0 * FX_SCALE
#define FX_WHITE_FREQ FX_DOUBLE(2300.0)   // 2300.0 * FX_SCALE
#define FX_FREQ_RANGE (FX_WHITE_FREQ - FX_BLACK_FREQ)  // ��Χ

// ԭʼ���㳣��������ȷ�����������
#define PI 3.14159265358979323846
#define SAMPLE_RATE 48000 // ������Ϊ48kHz

// SSTVɨ�����
#define Y_NUM 4224        // Y�źŲ������� (88ms * 48kHz)
#define Y_BEGINE 44256    // Y�źſ�ʼ������
#define Y_INTERVAL 7200   // Y�źż��

#define RY_NUM 2112       // RY�źŲ������� (44ms * 48kHz)
#define RY_BEGINE 48768   // RY�źſ�ʼ������
#define RY_INTERVAL 14400 // RY�źż��

#define BY_NUM 2112       // BY�źŲ������� (44ms * 48kHz)
#define BY_BEGINE 55968   // BY�źſ�ʼ������
#define BY_INTERVAL 14400 // BY�źż��

// SSTVƵ�ʷ�Χ����
#define SYNC_FREQ 1200
#define BLACK_FREQ 1500
#define WHITE_FREQ 2300
#define FREQ_RANGE (WHITE_FREQ - BLACK_FREQ)

// ͼ����
#define IMAGE_WIDTH 320   // ͼ����(����)
#define IMAGE_HEIGHT 240  // ͼ��߶�(����)
#define TOTAL_SAMPLES 1771680 // �ܲ�������

// BMP�ļ���ض���
#define BYTES_PER_PIXEL 3 // RGBÿ����3�ֽ�
#define FILE_HEADER_SIZE 14
#define INFO_HEADER_SIZE 40
#define PADDING_SIZE(width) ((4 - ((width) * BYTES_PER_PIXEL) % 4) % 4)

// ��������
void fm_demodulate_fx(short* I, short* Q, int length, short* freq_fx, int sample_rate);
void freq_to_yuv_fx(short* freq_fx, int length, short* Y_fx);
void map_to_pixels_fx(short* data_fx, int data_length, short* pixels_fx, int pixel_count);
void yuv_to_rgb(short Y, short RY, short BY, short* R, short* G, short* B);
void save_bmp(const char* filename, unsigned char* image_data, int width, int height);

// ������������
void extract_scan_line_fx(FILE* file_i, FILE* file_q, int start_sample, int num_samples, short* pixels_fx, int pixel_count);
void process_line_y_fx(FILE* file_i, FILE* file_q, int line_number, short* y_line_fx);
void process_line_ry_by_fx(FILE* file_i, FILE* file_q, int group_number, short* ry_line_fx, short* by_line_fx);

// �޸ĺ������������ڶ�ȡָ����Χ��IQ����
int read_iq_data_range_static(FILE* file, int offset, int count, short* buffer);

// ȫ���������� - ��Ϊ��������ʽ
short fx_y_lines[IMAGE_HEIGHT][IMAGE_WIDTH];      // �洢�����е�Yֵ(������)
short fx_ry_lines[IMAGE_HEIGHT/2][IMAGE_WIDTH];   // �洢RYֵ(������)
short fx_by_lines[IMAGE_HEIGHT/2][IMAGE_WIDTH];   // �洢BYֵ(������)

int main() {
    const char* output_file_path = "./output/sstv_decoded.txt";
    const char* bmp_file_path = "./output/sstv_image.bmp";
    const char* iq_data_i_path = "./sstv_data_file/sstv_iq_i_big_endian.bin"; // I�����ļ�·��
    const char* iq_data_q_path = "./sstv_data_file/sstv_iq_q_big_endian.bin"; // Q�����ļ�·��

    // ��IQ�����ļ��������ļ�ָ���
    FILE* file_i = fopen(iq_data_i_path, "rb");
    FILE* file_q = fopen(iq_data_q_path, "rb");

    if (file_i == NULL || file_q == NULL) {
        printf("�޷���IQ�����ļ�\n");
        if (file_i) fclose(file_i);
        if (file_q) fclose(file_q);
        return 1;
    }

    FILE *output_file = fopen(output_file_path, "w");
    if (output_file == NULL) {
        printf("�޷�������ļ�,�����д���/output�ļ����Դ������ļ�\n");
        fclose(file_i);
        fclose(file_q);
        return 1;
    }

    printf("��ʼSSTV����...\n");

    // ����ÿһ��Yֵ - ʹ�ö������汾
    for(int line = 0; line < IMAGE_HEIGHT; line++) {
        process_line_y_fx(file_i, file_q, line, fx_y_lines[line]);
        printf("����Y��: %d/240\r", line+1);
    }

    // ����ÿ���е�RY/BYֵ - ʹ�ö������汾
    for(int group = 0; group < IMAGE_HEIGHT/2; group++) {
        process_line_ry_by_fx(file_i, file_q, group, fx_ry_lines[group], fx_by_lines[group]);
        printf("����RY/BY��: %d/120\r", group+1);
    }

    // ������������ļ� - ���RGB��Ϣ���Ż���ʽ
    // fprintf(output_file, "# SSTV�������ݷ�������\n");
    // fprintf(output_file, "# ͼ��ߴ�: %d x %d ����\n", IMAGE_WIDTH, IMAGE_HEIGHT);
    // fprintf(output_file, "# ����ʱ��: %s\n\n", __DATE__);

    // fprintf(output_file, "# ���ݸ�ʽ˵��:\n");
    // fprintf(output_file, "# Y:    �����ź� (0-255)\n");
    // fprintf(output_file, "# R-Y:  ��ɫɫ�Ȳ��ź�\n");
    // fprintf(output_file, "# B-Y:  ��ɫɫ�Ȳ��ź�\n");
    // fprintf(output_file, "# R,G,B: ת�����RGB��ɫֵ (0-255)\n\n");

    // fprintf(output_file, "�к�,����,Yֵ,R-Yֵ,B-Yֵ,Rֵ,Gֵ,Bֵ\n");
    // fprintf(output_file, "--------------------------------------\n");

//    // ͳ����Ϣ����
//    double total_brightness = 0.0;
//    int dark_pixels = 0;
//    int bright_pixels = 0;

//    for(int line = 0; line < IMAGE_HEIGHT; line++) {
//        int group = line / 2;
//
//        for(int pixel = 0; pixel < IMAGE_WIDTH; pixel++) {
//            unsigned char R, G, B;
//
//            // ʹ�ö�������YUV��RGBת��
//            yuv_to_rgb_fx(
//                fx_y_lines[line][pixel],
//                fx_ry_lines[group][pixel],
//                fx_by_lines[group][pixel],
//                &R, &G, &B
//            );
//
//            // ����ͳ����Ϣ - ��Ҫת���ظ�����
//            total_brightness += TO_DOUBLE(fx_y_lines[line][pixel]);
//            if(TO_DOUBLE(fx_y_lines[line][pixel]) < 50) dark_pixels++;
//            if(TO_DOUBLE(fx_y_lines[line][pixel]) > 200) bright_pixels++;
//
//            // // ֻ���ÿ20�����ص����ݣ���ֹ�ļ�����
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

    // ���ͳ��ժҪ
    // fprintf(output_file, "\n\n# ͼ��ͳ��ժҪ\n");
    // fprintf(output_file, "ƽ������: %.2f\n", total_brightness / (IMAGE_WIDTH * IMAGE_HEIGHT));
    // fprintf(output_file, "����������(Y<50): %d (%.2f%%)\n",
    //         dark_pixels, (dark_pixels * 100.0) / (IMAGE_WIDTH * IMAGE_HEIGHT));
    // fprintf(output_file, "����������(Y>200): %d (%.2f%%)\n",
    //         bright_pixels, (bright_pixels * 100.0) / (IMAGE_WIDTH * IMAGE_HEIGHT));

    // ����RGBͼ��
    unsigned char* image_data = (unsigned char*)malloc(IMAGE_WIDTH * IMAGE_HEIGHT * BYTES_PER_PIXEL);
    if (image_data == NULL) {
        printf("�ڴ����ʧ��\n");
        fclose(output_file);
        return 1;
    }

    // ��YUVת��ΪRGB
    printf("\n��YUVת��ΪRGB...\n");
    for(int line = 0; line < IMAGE_HEIGHT; line++) {
        int group = line / 2;

        for(int pixel = 0; pixel < IMAGE_WIDTH; pixel++) {
            short R, G, B;

            // YUV��RGBת��
            yuv_to_rgb(
                fx_y_lines[line][pixel],
                fx_ry_lines[group][pixel],
                fx_by_lines[group][pixel],
                &R, &G, &B
            );

            // ��BMPͼ���У����ذ�BGR˳��洢
            int idx = (line * IMAGE_WIDTH + pixel) * BYTES_PER_PIXEL;
            image_data[idx] = B;     // B
            image_data[idx + 1] = G; // G
            image_data[idx + 2] = R; // R
        }
    }

    // ����ΪBMP�ļ�
    save_bmp(bmp_file_path, image_data, IMAGE_WIDTH, IMAGE_HEIGHT);

    printf("�������!\n");
    printf("- �����ѱ��浽: %s\n", output_file_path);
    printf("- ͼ���ѱ��浽: %s\n", bmp_file_path);

    // �ͷ���Դ
    free(image_data);
    fclose(file_i);
    fclose(file_q);
    fclose(output_file);

    return 0;
}

// ���������YUV��RGBת��
void yuv_to_rgb(short Y, short RY, short BY, short* R, short* G, short* B) {
    // ��׼YUV��RGBת��
	double r = 0.003906*((298.082*(Y - 16)) + (408.583*(RY - 128))); // YUV��RGBת��
	double g = 0.003906 * ((298.082 * (Y - 16.0)) + (-100.291 * (BY - 128.0)) + (-208.12 * (RY - 128.0)));
	double b = 0.003906 * ((298.082 * (Y - 16.0)) + (516.411 * (BY - 128.0)));

    // ������0-255��Χ��
	*R = (r < 0) ? 0 : ((r > 255) ? 255 : (short)r);
	*G = (g < 0) ? 0 : ((g > 255) ? 255 : (short)g);
	*B = (b < 0) ? 0 : ((b > 255) ? 255 : (short)b);

}

// ����BMP�ļ�
void save_bmp(const char* filename, unsigned char* image_data, int width, int height) {
    FILE* file = fopen(filename, "wb");
    if (file == NULL) {
        printf("�޷�����BMP�ļ�\n");
        return;
    }

    int padding_size = PADDING_SIZE(width);
    int stride = width * BYTES_PER_PIXEL + padding_size;
    int file_size = FILE_HEADER_SIZE + INFO_HEADER_SIZE + (stride * height);

    // BMP�ļ�ͷ
    unsigned char file_header[FILE_HEADER_SIZE] = {
        'B', 'M',                   // �ļ����ͱ�ʶ
        file_size & 0xFF,           // �ļ���С
        (file_size >> 8) & 0xFF,
        (file_size >> 16) & 0xFF,
        (file_size >> 24) & 0xFF,
        0, 0, 0, 0,                 // ����
        FILE_HEADER_SIZE + INFO_HEADER_SIZE & 0xFF,  // ��������ƫ��
        ((FILE_HEADER_SIZE + INFO_HEADER_SIZE) >> 8) & 0xFF,
        ((FILE_HEADER_SIZE + INFO_HEADER_SIZE) >> 16) & 0xFF,
        ((FILE_HEADER_SIZE + INFO_HEADER_SIZE) >> 24) & 0xFF
    };

    // BMP��Ϣͷ
    unsigned char info_header[INFO_HEADER_SIZE] = {
        INFO_HEADER_SIZE & 0xFF,             // ��Ϣͷ��С
        (INFO_HEADER_SIZE >> 8) & 0xFF,
        (INFO_HEADER_SIZE >> 16) & 0xFF,
        (INFO_HEADER_SIZE >> 24) & 0xFF,
        width & 0xFF,                        // ���
        (width >> 8) & 0xFF,
        (width >> 16) & 0xFF,
        (width >> 24) & 0xFF,
        height & 0xFF,                       // �߶�
        (height >> 8) & 0xFF,
        (height >> 16) & 0xFF,
        (height >> 24) & 0xFF,
        1, 0,                                // ��ɫƽ����
        BYTES_PER_PIXEL * 8, 0,              // ÿ����λ��
        0, 0, 0, 0,                          // ѹ����ʽ(��ѹ��)
        0, 0, 0, 0,                          // ͼ���С
        0, 0, 0, 0,                          // ˮƽ�ֱ���
        0, 0, 0, 0,                          // ��ֱ�ֱ���
        0, 0, 0, 0,                          // ��ɫ����ɫ��
        0, 0, 0, 0                           // ��Ҫ��ɫ��
    };

    // д���ļ�ͷ����Ϣͷ
    fwrite(file_header, 1, FILE_HEADER_SIZE, file);
    fwrite(info_header, 1, INFO_HEADER_SIZE, file);

    // ׼�������
    unsigned char padding[3] = {0, 0, 0};

    // BMP�ļ��Դ��µ��ϵ�˳��洢��
    for (int y = height - 1; y >= 0; y--) {
        // д��һ����������
        fwrite(&image_data[y * width * BYTES_PER_PIXEL], BYTES_PER_PIXEL, width, file);

        // ��������(�����Ҫ)
        if (padding_size > 0) {
            fwrite(padding, 1, padding_size, file);
        }
    }

    fclose(file);
}

// ʹ�ö������汾��ɨ������ȡ����
void extract_scan_line_fx(FILE* file_i, FILE* file_q, int start_sample, int num_samples, short* pixels_fx, int pixel_count) {
    // ʹ�þ�̬���������涯̬�ڴ����
    static short I_buffer[Y_NUM]; // ʹ������Y_NUM��Ϊ��������С
    static short Q_buffer[Y_NUM];
    static short freq_fx[Y_NUM];
    static short color_fx[Y_NUM];
    static short dummy1_fx[Y_NUM];
    static short dummy2_fx[Y_NUM];

    if(num_samples > Y_NUM) {
        printf("ɨ����������������������С\n");
        return;
    }

    // ��ȡIQ���ݵ���̬������
    if(!read_iq_data_range_static(file_i, start_sample, num_samples, I_buffer) ||
       !read_iq_data_range_static(file_q, start_sample, num_samples, Q_buffer)) {
        printf("�޷���ȡIQ����\n");
        return;
    }

    // ʹ�ö�������FM���
    fm_demodulate_fx(I_buffer, Q_buffer, num_samples, freq_fx, SAMPLE_RATE);

    // ʹ�ö�������Ƶ��תYUV
    freq_to_yuv_fx(freq_fx, num_samples - 1, color_fx);

    // ʹ�ö�������ӳ�䵽����
    map_to_pixels_fx(color_fx, num_samples - 1, pixels_fx, pixel_count);
}

// ������Yֵ - �������汾
void process_line_y_fx(FILE* file_i, FILE* file_q, int line_number, short* y_line_fx) {
    // ����Yɨ���ߵ���ʼλ��
    int start_sample = Y_BEGINE + (line_number * Y_INTERVAL);

    // ȷ���������źŷ�Χ
    if(start_sample + Y_NUM > TOTAL_SAMPLES) {
        printf("���棺�� %d ��Y�źų���������Χ\n", line_number);
        // �����ֵ
        for(int i = 0; i < IMAGE_WIDTH; i++) {
            y_line_fx[i] = 0;
        }
        return;
    }

    // ��ȡ���е�Yֵ - ʹ�ö������汾
    extract_scan_line_fx(file_i, file_q, start_sample, Y_NUM, y_line_fx, IMAGE_WIDTH);
}

// ����RY��BYֵ(ÿ���й���һ��) - �������汾
void process_line_ry_by_fx(FILE* file_i, FILE* file_q, int group_number, short* ry_line_fx, short* by_line_fx) {
    // ����RY��BYɨ�����ʼλ��
    int ry_start = RY_BEGINE + (group_number * RY_INTERVAL);
    int by_start = BY_BEGINE + (group_number * BY_INTERVAL);

    // ȷ���������źŷ�Χ
    if(ry_start + RY_NUM > TOTAL_SAMPLES) {
        printf("���棺�� %d ��RY�źų���������Χ\n", group_number);
        for(int i = 0; i < IMAGE_WIDTH; i++) {
            ry_line_fx[i] = 0;
        }
    } else {
        // ��ȡ�����RYֵ - ʹ�ö������汾
        extract_scan_line_fx(file_i, file_q, ry_start, RY_NUM, ry_line_fx, IMAGE_WIDTH);
    }

    if(by_start + BY_NUM > TOTAL_SAMPLES) {
        printf("���棺�� %d ��BY�źų���������Χ\n", group_number);
        for(int i = 0; i < IMAGE_WIDTH; i++) {
            by_line_fx[i] = 0;
        }
    } else {
        // ��ȡ�����BYֵ - ʹ�ö������汾
        extract_scan_line_fx(file_i, file_q, by_start, BY_NUM, by_line_fx, IMAGE_WIDTH);
    }
}

// ���������FM�������
void fm_demodulate_fx(short* I, short* Q, int length, short* freq_fx, int sample_rate) {
    static short fx_phase[Y_NUM];       // ʹ�þ�̬�������ջ���
    static int fx_unwrapped[Y_NUM];


    // ����ÿ�����������λ - ʹ��SB3500ƽ̨�ṩ��sb_fxatan����
    for(int i = 0; i < length; i++) {
        // sb_fxatan�����Χ��-32768��32767����Ӧ-�е���
        fx_phase[i] = sb_fxatan(I[i], Q[i]);
    }

    // ��λ���װ - ȷ����λ����
    fx_unwrapped[0] = fx_phase[0];
    for(int i = 1; i < length; i++) {
        int diff = fx_phase[i] - fx_phase[i-1];

        // ������λ���� (sb_fxatan����Цж�Ӧ32767)
        if(diff > SB_FXATAN_MAX) diff -= 2 * SB_FXATAN_MAX;
        else if(diff < -SB_FXATAN_MAX) diff += 2 * SB_FXATAN_MAX;

        fx_unwrapped[i] = fx_unwrapped[i-1] + diff;
    }

    // ����Ƶ�� - ��λ���
    // freq = (phase_diff * sample_rate) / (2*��)
    // ����sb_fxatan����Ѿ���һ��Ϊ-�е��еķ�Χ������ʹ�����������
    for(int i = 0; i < length - 1; i++) {
        // ��λ��
        int phase_diff = fx_unwrapped[i+1] - fx_unwrapped[i];

        // Ƶ�ʼ���: f = (����/2��) * sample_rate
        // ����������Ҫ����λ�����2�ж�Ӧ��sb_fxatanֵ(2*32767)��Ȼ����Բ�����
        freq_fx[i] = (phase_diff * sample_rate) / (2 * SB_FXATAN_PI_SCALE);
        //printf("freq_fx[%d]: %d\n", i, freq_fx[i]);
    }
}

// ���������Ƶ��תYUV����
void freq_to_yuv_fx(short* freq_fx, int length, short* Y_fx) {


    for(int i = 0; i < length; i++) {
        // ����SSTV��׼, ��Ƶ��ת��Ϊ����ֵ
        if(freq_fx[i] <= BLACK_FREQ) {
            Y_fx[i] = 0;
        } else if(freq_fx[i] >= WHITE_FREQ) {
            Y_fx[i] = 255;
        } else {
            // Y = ((freq - BLACK_FREQ) / FREQ_RANGE) * 255.0
            Y_fx[i] = ((freq_fx[i] - BLACK_FREQ) * 255 / FREQ_RANGE);
        }

        // ����RY��BY
        //R_Y_fx[i] = ((freq_fx[i] - BLACK_FREQ) / FREQ_RANGE) * 255 - 128;
        //B_Y_fx[i] = ((freq_fx[i] - BLACK_FREQ) / FREQ_RANGE) * 255 - 128;
    }
}

// �������������ӳ�䵽����
void map_to_pixels_fx(short* data_fx, int data_length, short* pixels_fx, int pixel_count) {
    // ʹ����������ÿ�����ض�Ӧ�Ĳ�������
    //double samples_per_pixel_fx = (double)data_length / pixel_count;
	//����samples_per_pixel_fxֻ����ȡ����ֵ���ֱ��Ӧdata_lengthΪ4224-1��2112-1����ǰ�������ֵ���Խ��͸���������Դռ��
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
        // �������ض�Ӧ�Ĳ����㷶Χ
        int start_idx = pixel * samples_per_pixel_fx;
        //int end_idx = ((pixel + 1) * (samples_per_pixel_fx - 1)) - 1;
        int end_idx = start_idx + samples_per_pixel_fx - 1;

        // �߽���
        if(end_idx >= data_length) end_idx = data_length - 1;
        if(start_idx < 0) start_idx = 0;

        // �ۼ����ط�Χ�ڵ�����ֵ
        int sum = 0;
        int count = 0;

            for(int i = start_idx; i <= end_idx; i++) {
                sum += data_fx[i];
                count++;
            }

        // ����ƽ��ֵ
        pixels_fx[pixel] = (count > 0) ? (sum / count) : 0;
    }
}

// �Ż����ļ���ȡ���� - ʹ�þ�̬������
int read_iq_data_range_static(FILE* file, int offset, int count, short* buffer) {
    if (file == NULL) {
        printf("�ļ�ָ��Ϊ��\n");
        return 0;
    }

    long point = (long)(offset * sizeof(short));
    //long point = (long)0;
    // ��λ���ļ��е�ָ��λ��
    if (fseek(file, point, SEEK_SET) != 0) {
        printf("�ļ���λʧ�ܣ�ƫ����: %ld\n", offset);
        return 0;
    }

    // ��ȡָ�����������ݵ��ṩ�Ļ�����
    size_t read_count = fread(buffer, sizeof(short), count, file);
    //*(buffer) = (*(buffer) & 0x00FF) << 8 | (*(buffer) & 0xFF00) >> 8;
    if (read_count != count) {
        printf("��ȡ����ʧ��: Ԥ�� %d �ʵ�ʶ�ȡ %zu ��\n", count, read_count);
        return 0;
    }

    return 1;
}
