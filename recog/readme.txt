Version1：

目前功能
vis同步头识别（没有考虑噪声频偏以及衰减）

文件说明：
vis_generatev2 的part1用于生成一段纯粹的vis同步头，在后面的识别中没什么用，可以忽略
vis_generatev2 的part2用于生成一段音频，在这段音频中，vis同步头的前610ms被随机插入到一段噪声中，音频的采样率为6826（在识别阶段为了满足信号中1900hz的部分，采样率必须大于等于3800hz，而FFT的周期为150ms，则在该周期中最少需要采样570个样本点，那么FFT选取1024个点。由于模拟时我们需要用一个数字信号来代替实际应用中被采集的模拟信号，所以音频的采样率为6826=1024/0.15）

Vis_v1 用来识别上一个文件生成的音频中的vis同步头。简单测试了一下，精度大概在0.001s

问题：
Vis_v1 的识别是由2部分组成的，即先粗略搜索，再精细搜索。按理来说第一部分的采样率应该低于第二部分，但是我这里用的是同一个音频，也就是说采样率保持一致。我不知道这样会不会影响识别。
