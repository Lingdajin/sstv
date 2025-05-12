[sig,fs] = audioread('sync_embedded.wav');

%%

window_size = round(0.001 * fs);   % 1ms窗
step_size = window_size;           % 无重叠
n_windows = floor((length(sig) - window_size) / step_size);

freqs = (0:window_size-1) * fs / window_size;
energy = zeros(1, n_windows);

for i = 1:n_windows
    segment = sig((i-1)*step_size + 1 : i*step_size);
    fft_result = abs(fft(segment));

    [~, idx_1200] = min(abs(freqs - 1200));
    energy(i) = fft_result(idx_1200);  % 只取1200Hz幅度
end

% 找峰值位置
[max_val_fft, max_idx_fft] = max(energy);
start_sample_fft = (max_idx_fft - 1) * step_size + 1;
start_time_fft = start_sample_fft / fs;

% 绘图
figure;
plot(energy);
title('FFT滑窗 - 1200Hz 幅度');
xlabel('窗编号');
ylabel('幅度');
hold on;
plot(max_idx_fft, max_val_fft, 'ro');
text(max_idx_fft, max_val_fft, sprintf('← 起点 %.4fs', start_time_fft));
hold off;


%%

N = window_size;
n_windows = floor((length(sig) - N) / N);
energy_goertzel = zeros(1, n_windows);

k_1200 = round(1200 * N / fs);

for i = 1:n_windows
    segment = sig((i-1)*N + 1 : i*N);
    g_1200 = goertzel(segment, k_1200);
    energy_goertzel(i) = abs(g_1200);
end

% 找峰值
[max_val_g, max_idx_g] = max(energy_goertzel);
start_sample_g = (max_idx_g - 1) * N + 1;
start_time_g = start_sample_g / fs;

% 绘图
figure;
plot(energy_goertzel);
title('Goertzel滑窗 - 1200Hz 幅度');
xlabel('窗编号');
ylabel('幅度');
hold on;
plot(max_idx_g, max_val_g, 'ro');
text(max_idx_g, max_val_g, sprintf('← 起点 %.4fs', start_time_g));
hold off;


%%

% 构造模板：1200Hz 9ms + 1500Hz 3ms
t1 = 0:1/fs:0.009 - 1/fs;
t2 = 0:1/fs:0.003 - 1/fs;
template = [sin(2*pi*1200*t1), sin(2*pi*1500*t2)];
template = template / norm(template);  % 归一化

% 匹配滤波（相关）
corr_result = conv(sig, fliplr(template), 'valid');
corr_result = corr_result / max(abs(corr_result));

% 查找最大值
[max_val_corr, max_idx_corr] = max(corr_result);
start_sample_corr = max_idx_corr;
start_time_corr = start_sample_corr / fs;

% 绘图
figure;
plot(corr_result);
title('匹配滤波 - 1200Hz+1500Hz 序列');
xlabel('样本点');
ylabel('归一化相关');
hold on;
plot(max_idx_corr, max_val_corr, 'ro');
text(max_idx_corr, max_val_corr, sprintf('← 起点 %.4fs', start_time_corr));
hold off;

