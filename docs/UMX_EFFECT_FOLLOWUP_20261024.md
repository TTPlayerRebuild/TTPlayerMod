# 真实 UMX 与两组 XM 效果对照（2026-10-24）

## 结论

本轮新增一个实际地图包内的 UMX 和两组上游 XM 效果样本。原 DLL 与默认兼容构建均成功全量解码和定位，三组音序时长、EOF PCM 长度相同，**PCM 内容仍有差异**。这些结果扩大了可载入容器和音序覆盖，不能标为全部 Tracker 效果或混音已逐采样恢复。

本轮没有修改生产代码、切换内核或重新发行 DLL；避免用尚未定位原因的波形差异驱动猜测性的参数修改。

## 样本与来源

1. `ASP-Botmca9.umx`：来自 [Unreal Archive 的 DOM-UTCathode 地图包](https://unrealarchive.org/unreal-tournament/maps/domination/U/dom-utcathode_2f4d6a29.html)，不是本地生成的 UMX。归档 SHA-1 `2f4d6a2909ab66ca4918839959b04d21963ae623`；UMX SHA-1 `9219c79890131efba67f944259e0c4f4246e2bff`，两项均与来源清单相符。UMX 大小 1,213,812 字节，版本 68，内嵌 IT 签名在偏移 166。本次没有运行地图内容，也没有将此文件当作未经修改的原游戏发行文件。
2. [finefx_ft2.xm](https://github.com/libxmp/libxmp/blob/5d4f6aa2c7dcdf8e9cd8129fe579d45b36a302da/test-dev/data/finefx_ft2.xm)：FT2/XM 下的 S3M 风格细微效果兼容案例；用途见其[上游测试说明](https://github.com/libxmp/libxmp/blob/5d4f6aa2c7dcdf8e9cd8129fe579d45b36a302da/test-dev/test_effect_finefx_ft2.c)。
3. [ft2_double_toneporta.xm](https://github.com/libxmp/libxmp/blob/5d4f6aa2c7dcdf8e9cd8129fe579d45b36a302da/test-dev/data/ft2_double_toneporta.xm)：音量栏 `Mx` 与 `3xx/5xy` 同行出现时的双重滑音推进和速度记忆案例；用途见其[上游测试说明](https://github.com/libxmp/libxmp/blob/5d4f6aa2c7dcdf8e9cd8129fe579d45b36a302da/test-dev/test_player_ft2_double_toneporta.c)。

这里借用上游文件覆盖特定命令组合，比较对象仍是千千静听原 DLL，没有将 libxmp 的现代参考渲染规则强加给重建版。

## 实测

Windows 11 本机，独立 x86 探针，固定 4096 字节读取；两侧均输出 44100 Hz / 16-bit / stereo。

| 样本 | 两侧时长 ms | 两侧 EOF 字节数 | 原/新不同采样值数量 | 最大整数差 |
| --- | ---: | ---: | ---: | ---: |
| finefx_ft2.xm | 1440 | 274432 | 49365 | 4315 |
| ft2_double_toneporta.xm | 4256 | 774144 | 169963 | 4317 |
| ASP-Botmca9.umx | 357449 | 63188992 | 31584305 | 18270 |

两侧各检查 0、中点和曲尾定位后的读取，全部成功；定位沿用原版音序比例语义，不代表逐采样精确 Seek。

每个样本随后在新进程内重复解码：

- 重建 DLL 三组均与自身前次输出逐字节相同。
- 原 DLL 的 UMX 与自身前次输出相同；两个 XM 分别有 22、1306 个采样值变化，最大差值 54、69。
- 原/新的主要差异大于上述原 DLL 重复波动，不能仅以抖动或随机种子解释。当前证据尚未把差异拆分到音序效果、重采样、声像、滤波或混音累加中的某一项。

后续应在相同曲目位置比较 row/tick、周期、音量和声像，再比较重采样前后的样本。不能仅凭总时长一致宣称效果完全一致，也不应以换用现代 libopenmpt 消除或掩盖历史差异。

## 证据范围

原 DLL SHA-256：`e295817c9532d1a159a470b2caa6cb2dfc0bef7930f56d0d9ffacb05b1b8c036`。

重建 DLL SHA-256：`aa22382ba626fb500ec8a0c7f8fe10f1d1f89d0f0f58101567bc10cc2536ce21`，本轮未改变。

样本、来源清单、日志、PCM 和比较脚本保存在工作区 `rebuild/tests/plugins_followup_20261024/legacy`。本轮没有重跑 XP/Win7、播放器界面或现代可选内核，也没有穷尽 UMX 版本和效果指令。测试与外部音乐不进入插件仓库、Action 或发行包。
