% ==== 参数 ====
fs = 4600;       % 采样率
A = 0.8;         % 正弦幅度
noise_A = 0.4;   % 噪声幅度

% 频率及持续时间设置
f_list = [1200, 1500, NaN, 1500, 1900, NaN];       % NaN 表示噪声段
d_list = [0.009, 0.003, 0.088, 0.0045, 0.0015, 0.044];  % 每段持续时间（秒）


% ==== 构造整个信号 ====
sig = [];
phase = 0;

for i = 1:length(f_list)
    f = f_list(i);
    d = d_list(i);
    if isnan(f)   % 噪声段
        n = round(d * fs);
        y = noise_A * (2 * rand(n,1) - 1);  % 白噪声 [-A,A]
    else
        [y, phase] = generate_sine(fs, f, d, A, phase);
    end
    sig = [sig; y];
end

% ==== 播放与保存 ====

audiowrite('composite_signal.wav', sig, fs);  % 保存到文件


%%

% 将信号随机插入到一段噪声中

duration_total = 5; % 总时长 5 秒
N_total = round(fs * duration_total);

% 生成背景噪声（均值0，方差0.1）
background = 0.1 * randn(N_total, 1); 

target_signal = sig;
target_len = length(target_signal);


% ==== 插入到背景中某个位置 ====
insert_pos = randi([fs, N_total - target_len - fs]); % 避免靠近头尾
signal_with_target = background;
signal_with_target(insert_pos : insert_pos + target_len - 1) = ...
    signal_with_target(insert_pos : insert_pos + target_len - 1) + target_signal;

% ==== 保存为WAV文件（用于识别测试）====
audiowrite('sync_embedded.wav', signal_with_target, fs);
