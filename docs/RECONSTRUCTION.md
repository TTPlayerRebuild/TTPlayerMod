# Modules 恢复与现代内核

最新：[2026.10.29p2 UMX 输入范围检查](UMX_INPUT_BOUNDS_20261029P2.md)，修正容器偏移和长度校验；三系统普通接口各 111 项通过。

此前：[2026-10-29 真实 UMX 采样末端修复](UMX_SAMPLE_GUARD_20261029.md)，已恢复原末端保护规则并使所测真实 UMX 的完整 PCM 与原版相等。随后 [XM 剩余差异因果验证](XM_NUMERICS_20261029P1.md) 确认两组 XM 残差来自原版未初始化的采样前缀，保留重建版安全初始化；该轮未修改 `2026.10.29` 生产 DLL。

日期：2026-10-19。

后续：[真实 UMX 与两组 XM 效果对照（2026-10-24）](UMX_EFFECT_FOLLOWUP_20261024.md)。新增样本的音序时长和 EOF 长度一致，但 PCM 仍有差异，未将其标记为完全恢复。

原 DLL SHA-256：`e295817c9532d1a159a470b2caa6cb2dfc0bef7930f56d0d9ffacb05b1b8c036`。

## 原版证据

- 一个 Modules Reader，八种扩展名。二进制里有未参与工厂枚举的 CMIDReader／DirectMusic 残留，不把它误判成有效 MIDI 工厂。
- `60452929` 使用 IStream::Stat 取文件名，然后 Unicode→ACP、CreateFileA 和整文件映射，忽略宿主流内容。
- 同函数固定 PCM 44100／16／2，并调用 `60455497` 设置混音；`6045545c(3)` 设置旧重采样模式。
- `60452c50` 读取混音数据，原建议缓冲 4096 字节；`60452d29` 将毫秒按内部最大位置比例换算，并非样本精确定位。
- `60452dae` 返回 E_NOTIMPL，对应不可修改的标题元数据操作。
- `604678fc` 是旧 UMX 启发式分支：要求导出表偏移位于文件末尾 512 字节内，从偏移 0x40 到 0x4ff 扫描 IMPM／SCRM／Extended Module／M.K.，再调用对应模块读取器；它没有完整解释 Unreal 对象表。新内核按 UMX 的名字、导入与导出表查找 Music 对象。

## 默认兼容核

默认采用原谱系 libmodplug 0.8.9.0；内部直接使用 CSoundFile，对象不跨 DLL ABI。公开仍是原 Reader／Metadata 虚表。

上游 GetLength 原本以 `(dwElapsedTime+500)/1000` 返回整秒。根据 `604562bd` 恢复累计毫秒值，绕开整秒 C API。Seek 按 `60452d29` 使用请求毫秒与最大音序位置的比例，再调用 SetCurrentPos，不宣称样本精确定位。

固定波形参数与 resampling=3。IStream 短读到完整模块后载入，输入上限 256 MiB；不根据 Stat 文件名重新打开。标题按原 ANSI 规则读取，修改返回 E_NOTIMPL。仅启用声明的八种模块／容器分派，不启动 MIDI／PAT 外部音色库路径。

底层混音固定用原推荐的 4096 字节块，向调用者按帧分段返回，使不同容量不改变旧核的末尾填充。不足 4 字节拒绝，Stop 返回 E_ABORT，Start 可继续。

### 并发隔离

旧核有全局混音缓冲、波形设置和 ramp/DC 历史。CRITICAL_SECTION 保护 Create／Read／Seek／Destroy；固定配置只初始化一次。四个 dry/reverb offset 状态保存在每个 Reader，读取和定位前后恢复与保存，避免仅加锁仍导致交错读取污染。

私有探针比较独立单 Reader 与两条并发播放线程，第三线程反复创建、查询并释放模块。各循环 20 次，逐字节 PCM 一致；17／4096／8193 三种容量同样一致。

## 原版对照

| 样本 | 原版与默认兼容核时长 ms | 两者 EOF PCM 字节数 |
| --- | ---: | ---: |
| 合成 MOD | 7680 | 1376256 |
| 上游 XM | 8544 | 1544192 |
| 上游 S3M | 2124 | 393216 |
| libxmp IT／合成 UMX | 4800 | 868352 |
| FAR effect1 | 5984 | 1077248 |
| MTM TEMPO | 8533 | 1531904 |
| STM pattern jump / break | 260 | 65536 |

不因此声称全部 PCM 与原二进制相同；混音、CPU 路径与全部效果命令仍需更多曲库对照。IT/FAR/MTM/STM 样本来自 libxmp 固定提交 `5d4f6aa2c7dcdf8e9cd8129fe579d45b36a302da`；UMX 是合法 Unreal v63 Music 对象的合成包。八种扩展验证 EOF、定位、短读、虚拟流和容量边界。

## 可选现代核差异

TTP_MOD_MODERN_ENGINE=ON 显式选用 libopenmpt 0.8.9，不作为默认或自动回退。其 S3M 测试曲变为 1231392 ms、MTM 为 10889 ms、STM 为 1599 ms，不能当作原版兼容修复；公开 ctl 没有整体恢复旧 ModPlug 音序的开关。

现代核默认 dither=1 随机播种，同容量重复载入的 PCM 也不同，实测最多相差两个 16-bit LSB。现选择 dither=2（原 ModPlug 0.5-bit、固定种子），同系统容量与重复载入稳定。XP／Win7 混音仍可能不同，未声称跨系统逐字节一致。

## 验证范围

测试仅在工作区 rebuild/tests/audio_plugins_fix_20261019/legacy_open，不进入 Git 或 Action。仍需扩大实际 UMX 游戏包、完整效果命令、损坏码流与原宿主界面覆盖。旧系统动态结果以主任务集成记录为准。

默认兼容核另做 40 个截断输入的进程级检查，未发生崩溃或超时；这是有限边界覆盖，不等于所有损坏输入已穷尽。现代可选构建在独立构建目录成功编译，并通过 XP／Win7 静态导入检查；普通 Release 始终使用默认兼容核。

## XP／Win7 最终虚拟机回归

2026-10-19：最终 R4 探针包在 XP 与 Win7 完整运行，均 `FAILURES=0`。记录保存在私有测试目录 `xp-r4-results.txt`、`win7-r4-results.txt`。包含原 DLL 对照、播放／定位／缓冲边界、MPC 保存和模拟失败恢复、MOD 双播放 Reader 与后台扫描并发。MOD 八种样本的时长及 EOF 字节数也在两个系统分别核对原版一致。原版 DLL 使用仅限测试目录的 VC2012 私有运行库；重建 DLL 不依赖该库。

这批验证使用最终运行 ZIP 内相同 SHA-256 的 DLL。

## 最终真实播放器验证

2026-10-19：采用默认兼容核的最终 MOD DLL 在原版 5.7.9 与重建播放器中分别完成 Windows 11、XP SP3、Win7 SP1 的加载、开始播放与关闭测试。六次均加载预期插件路径，主窗口存在、没有错误对话框、没有强制关闭，PlayingTime 大于零。私有记录：`rebuild/tests/audio_plugins_fix_20261019/hosts/{win11,xp,win7}-final-legacy.log`。

这确认真实宿主 ABI 与播放入口，不扩大为所有模块效果命令均相同。Windows 11 不能替代独立 Windows 10 实测。

## 2026-10-25 混音差异修复

已恢复原版主音量/预放大、三组滤波表、立体声插值、抖动和 16 位取整；原 DLL 99,328 个系数与 2,048 个抖动输出精确对照通过。11 个样本及每系统 14 组 XP / Win7 回归通过。详见 [MIXER_PARITY_20261025.md](MIXER_PARITY_20261025.md)，其中明确保留真实 UMX 的局部 PCM 差异，未宣称全效果逐位一致。
