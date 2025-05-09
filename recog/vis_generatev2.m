
%part1

%该部分产生一个纯粹的vis同步头

fs = 6826;
t1 = 0:1/fs:0.3 - 1/fs;    % 300ms
t2 = 0:1/fs:0.01 - 1/fs;   % 10ms
t3 = 0:1/fs:0.3 - 1/fs;    % 300ms

% 第1段：1900Hz，起始相位为0
f1 = 1900;
phi1 = 0;
s1 = sin(2*pi*f1*t1 + phi1);
% 记录结束相位
phi1_end = mod(2*pi*f1*t1(end) + phi1, 2*pi);

% 第2段：1200Hz，起始相位 = 第1段结束相位
f2 = 1200;
s2 = sin(2*pi*f2*t2 + phi1_end);
phi2_end = mod(2*pi*f2*t2(end) + phi1_end, 2*pi);

% 第3段：1900Hz，起始相位 = 第2段结束相位
s3 = sin(2*pi*f1*t3 + phi2_end);

% 合并信号
signal = [s1, s2, s3];

audiowrite('test_1900_1200_1900.wav', signal, fs);
%%


[S, F, T, P] = spectrogram(signal, 256, 250, 256, fs);

% 绘制时频图
figure;
surf(T, F, 10*log10(abs(P)), 'EdgeColor', 'none');
axis tight;
view(0, 90); % 设置视角为俯视图
xlabel('Time (seconds)');
ylabel('Frequency (Hz)');
title('Spectrogram of WAV File');
colorbar;


%%

% part2

% 该部分将vis同步头随机插入到一段噪音中

% 基本参数
fs = 6826;
duration_total = 5; % 总时长 5 秒
N_total = round(fs * duration_total);

% 生成背景噪声（均值0，方差0.1）
background = 0.1 * randn(1, N_total);

% ==== 生成目标信号（相位连续） ====
t1 = 0:1/fs:0.3 - 1/fs;
t2 = 0:1/fs:0.01 - 1/fs;
t3 = 0:1/fs:0.3 - 1/fs;

f1 = 1900; f2 = 1200;
phi1 = 0;
sig1 = sin(2*pi*f1*t1 + phi1);
phi1_end = mod(2*pi*f1*t1(end) + phi1, 2*pi);

sig2 = sin(2*pi*f2*t2 + phi1_end);
phi2_end = mod(2*pi*f2*t2(end) + phi1_end, 2*pi);

sig3 = sin(2*pi*f1*t3 + phi2_end);
target_signal = [sig1, sig2, sig3];
target_len = length(target_signal);

% ==== 插入到背景中某个位置 ====
insert_pos = randi([fs, N_total - target_len - fs]); % 避免靠近头尾
signal_with_target = background;
signal_with_target(insert_pos : insert_pos + target_len - 1) = ...
    signal_with_target(insert_pos : insert_pos + target_len - 1) + target_signal;





% ==== 保存为WAV文件（用于识别测试）====
audiowrite('test_signal_embedded.wav', signal_with_target, fs);


%%

[S, F, T, P] = spectrogram(signal_with_target, 256, 250, 256, fs);

% 绘制时频图
figure;
surf(T, F, 10*log10(abs(P)), 'EdgeColor', 'none');
axis tight;
view(0, 90); % 设置视角为俯视图
xlabel('Time (seconds)');
ylabel('Frequency (Hz)');
title('Spectrogram of WAV File');
colorbar;
