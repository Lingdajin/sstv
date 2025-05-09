% 参数设定
fs = 6826;       % 采样率
f_target = 1900;    % 目标频率
frame_len = 1024;   % 每帧点数（150ms）
NFFT = 1024;        % FFT点数，与帧长一致

%% Step 1：生成理想1900Hz信号，计算功率谱能量阈值
t = (0:frame_len-1)/fs;
ideal_sig = sin(2*pi*f_target*t) .* hamming(frame_len)';

% 计算FFT结果并计算功率谱
fft_ideal = fft(ideal_sig, NFFT);
P_ideal = abs(fft_ideal(1:NFFT/2+1)).^2;  % 计算功率谱
bin_1900 = round(f_target * NFFT / fs);   % 1900Hz对应的频率bin
energy_per_frame = P_ideal(bin_1900);      % 单帧1900Hz的功率谱能量
threshold_energy = 3 * energy_per_frame;   % 阈值 = 3帧1900Hz总能量

fprintf("阈值能量（3帧1900Hz）：%.4f\n", threshold_energy);

%% Step 2：生成测试信号或导入数据
[sig,Fs] = audioread('test_signal_embedded.wav');

% 零补至1024整数帧
pad = mod(-length(sig), frame_len);
sig = sig(:).';
sig = [sig, zeros(1, pad)];

%% Step 3：逐帧计算1900Hz频率的功率谱能量
num_frames = length(sig) / frame_len;
energies = zeros(1, num_frames);

for i = 1:num_frames
    frame = sig((i-1)*frame_len + 1 : i*frame_len);
    frame = frame .* hamming(frame_len)';  % 加窗
    fft_result = fft(frame, NFFT);         % 计算FFT
    P = abs(fft_result(1:NFFT/2+1)).^2;   % 计算功率谱
    energies(i) = P(bin_1900);             % 提取1900Hz频率对应的能量
end

%% Step 4：滑动窗口检测连续4帧中总能量 > 阈值
found = false;
for i = 1:(num_frames - 3)
    total_energy = sum(energies(i:i+3));  % 连续4帧能量之和
    if total_energy > threshold_energy
        found = true;
        fprintf("识别到信号，起始帧 = %d\n", i);
        break;
    end
end

if ~found
    fprintf("未识别到信号。\n");
end

%% 可视化
figure;
stem(energies, 'filled');
xlabel('帧编号');
ylabel('1900Hz 信号能量（功率谱）');
title('各帧1900Hz频率分量的能量');
yline(energy_per_frame, '--r', '单帧参考能量');
yline(threshold_energy, '--g', '3帧总能量阈值');


%%

% 时间同步

fs = 6826;  % 采样率
t1 = 0:1/fs:0.005 - 1/fs;  % 5ms -> 1900Hz
t2 = 0:1/fs:0.010 - 1/fs;  % 10ms -> 1200Hz
t3 = 0:1/fs:0.005 - 1/fs;  % 5ms -> 1900Hz

% 相位连续构建模板
phi1 = 0;
sig1 = sin(2*pi*1900*t1 + phi1);
phi1_end = mod(2*pi*1900*t1(end) + phi1, 2*pi);
sig2 = sin(2*pi*1200*t2 + phi1_end);
phi2_end = mod(2*pi*1200*t2(end) + phi1_end, 2*pi);
sig3 = sin(2*pi*1900*t3 + phi2_end);
template = [sig1, sig2, sig3];
template = template / norm(template);  % 归一化

% 假设目标信号为 sig，搜索范围为 rough_start 到 rough_end
% 你需要确保 sig 是一段音频数据（如 wav 文件读入）
% i 表示大致窗口编号
rough_start = round((i * 0.15) * fs);   
rough_end   = round(((i + 4) * 0.15) * fs);   

segment = sig(rough_start:rough_end);  % 取目标段

% --- 点积相关性计算 ---
segment_len = length(segment);
template_len = length(template);
num_positions = segment_len - template_len + 1;
corr_result = zeros(1, num_positions);

for k = 1:num_positions
    window = segment(k : k + template_len - 1);
    corr_result(k) = sum(window .* template);
end

% 找最大相关点
[~, max_idx] = max(corr_result);
offset = max_idx;
sync_pos = rough_start + offset - 1;
start_1200hz = sync_pos + length(sig1);

% --- 打印结果 ---
fprintf("✅ 模板匹配完成：最大相关点位置为第 %d 个采样点（%.4f 秒）\n", sync_pos, sync_pos / fs);
fprintf("🎯 1200Hz 起点约为第 %d 个采样点（%.4f 秒）\n", start_1200hz, start_1200hz / fs);

%% --- 可视化 ---
t_segment = (0:length(segment)-1)/fs + rough_start/fs;
t_corr = (0:num_positions-1)/fs + rough_start/fs;

figure;

subplot(2,1,1);
plot(t_segment, segment);
hold on;
xline(sync_pos/fs, 'r--', 'LineWidth', 1.5);
xline(start_1200hz/fs, 'g--', 'LineWidth', 1.5);
legend('信号片段', '匹配起点', '1200Hz起点');
title('信号段及匹配结果');
xlabel('时间 (秒)');
ylabel('幅度');
grid on;

subplot(2,1,2);
plot(t_corr, corr_result);
hold on;
xline(sync_pos/fs, 'r--', 'LineWidth', 1.5);
title('滑动点积相关性');
xlabel('时间 (秒)');
ylabel('相关值（点积）');
grid on;



