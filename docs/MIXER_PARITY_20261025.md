# 2026-10-25：原版 MOD 混音公共路径恢复

## 结论

修复了上轮真实 UMX 与两组 XM 差异中的四项通用原因：模块初始主音量/预放大、重采样系数与分派、16 位抖动、最终整数取整。没有为某个文件名、时长或效果编号添加特例，没有改动 tracker 的 row/tick 效果解释。

两组 XM 已基本收敛到原 DLL 自身重复运行时的边界波动；真实 UMX 仍有局部 PCM 差异。不能据此宣称所有模块已逐采样完全一致。

## 原 DLL 证据与根因

目标为根目录 `AddIn/ttp_mod.dll`，SHA256：

`e295817c9532d1a159a470b2caa6cb2dfc0bef7930f56d0d9ffacb05b1b8c036`

本轮使用 REA 6.5.0 CLI / Ghidra 12.1.3 做只读分析。完整 Evidence 保存在私有 `rebuild/tests/plugins_followup_20261025/mod/original-*.json`。没有改写原 DLL；地址和伪代码是分析观察，不是原始源码。

| 原函数 | 原版行为 | 原重建实现的差异 | 修复 |
|---|---|---|---|
| `60453b96` Create | 主音量 256、默认预放大 100，再执行格式加载器和按声道数限制预放大 | libmodplug 0.8.9.0 为 128 / 48 | 在生成源码中恢复初始值；保留 IT 自带预放大及后续上限 |
| `6045545c` | 模式 3 设置 `0x410` | 编号相同，第三方内部算法不同 | 保留接口，恢复实际混音算法 |
| `60467a5f`、`60467d25`、`60467e62` | 两组 4096 相位、8 tap Kaiser 表，beta/cutoff 分别为 9.6377/1.0、8.5/.75 | 使用 Blackman 窗 FIR | 根据原公式生成，保留截断与量化顺序 |
| `60467abd`、`604679cc` | 第三组为半步长 cubic lowpass，系数缩放 8192 | 统一使用 Blackman FIR | 根据完整汇编确认参数与写表顺序后生成 |
| `6046816a`、`60468295` | 单声道按 `abs(nInc)` 的 `0x13000`、`0x18000` 阈值选表 | 未区分三组滤波表 | 8 / 16 位及反向播放按原阈值分派 |
| `6046986f`、`60468be2` | 立体声 HQ/FIR 使用 256 相位 cubic，系数向零截断 | FIR 或 1024 相位 cubic | 恢复立体声分派和 256 相位系数 |
| `604592f2`、`60469ece` | HQ 条件下加入有符号抖动 | 没有调用抖动 | 恢复相同整数状态更新及有符号右移 |
| `60469d7e` | 加 `0x800`、限幅，再右移 12 位 | x86 路径向零截断 | 恢复取整与限幅顺序 |

关键 Evidence ID：

- Create：`ev_73f67e2d8c1ae2d0f1aefe70bb8ad76e5c2894e33b5f3cd2b976857eee9dc5d0`
- Kaiser：`ev_b58ddb81e6e5202a64c233c7d6d043ea21c6a8a7f2ea3139a4bc84b6006f5492`
- 第三组表的完整函数及汇编：`ev_f8802336d797d06e9573df54028b74dd0cf03ebab10aa3c85c6e3bb2c3953b9f`
- 抖动：`ev_49ac8f234c255e4a3fad67b5d8b9ee7305058b84994e84f92e6ba2e199589740`
- 取整：`ev_2097ceca210d1d7bce8ed22b1d914c20aea99655deea4c28578583c0f104e76f`

反编译器将抖动右移显示为无符号操作；独立运行原函数后发现实际输出包含负数。实现采用运行证据确认的有符号右移，没有仅凭反编译类型照搬。

## 实现与 Reader 隔离

- `src/legacy_mixer.*` 保存数学公式和整数运算，没有复制 DLL 系数字节。
- `cmake/mixer_compat.cmake` 对固定版本上游源码执行边界校验后替换；移除未使用的 Blackman 表生成，避免多余初始化和体积。
- `src/reader.cpp` 在现有引擎初始化门内只生成一次系数，实际读、定位、创建、销毁继续使用现有临界区。
- 两个抖动状态与 DC/ramp 状态一样按 Reader 保存，进入引擎前恢复、退出后保存。新 Reader 从零开始，不继承其他 Reader 的状态。
- Seek 保留该 Reader 的抖动进度；Stop 后失败读取不消耗状态。两者都不改变其他 Reader 的状态。
- 可选 `TTP_MOD_MODERN_ENGINE=ON` 的 libopenmpt 路径不应用这些旧内核补丁。

## 验证

测试代码、样本、PCM 和 Evidence 仅保存在私有 `rebuild/tests/plugins_followup_20261025/mod/`，不进入仓库或 Action。

### 独立原 DLL 对照

`table_probe.exe` 通过固定原 DLL 的工厂打开模块后，对照其运行时数据及函数：

- 三组 4096 × 8 系数及 256 × 4 立体声系数：**99,328 个系数全部相等**。
- 独立调用原 DLL 抖动函数，与重建函数比较 2,048 个整数输出：**全部相等**。
- 实测原版 `sysinfo=0`、混音标记 `0x410`、最大混音声道数 32；固定参数与本次原版运行路径一致。

### 完整播放与 ABI

11 个样本分别用原 DLL 与重建 DLL 完整读取，格式、EOF、缓冲区边界、0/中点/末尾定位通过，全部时长与 PCM 字节数和原版一致：

| 样本 | 时长 ms | PCM 字节数 |
|---|---:|---:|
| tone.mod | 7,680 | 1,376,256 |
| libxmp.far | 5,984 | 1,077,248 |
| libxmp.it / fixture.umx | 4,800 | 868,352 |
| libxmp.mtm | 8,533 | 1,531,904 |
| libxmp.stm | 260 | 65,536 |
| upstream-test.s3m | 2,124 | 393,216 |
| upstream-test.xm | 8,544 | 1,544,192 |
| finefx_ft2.xm | 1,440 | 274,432 |
| ft2_double_toneporta.xm | 4,256 | 774,144 |
| ASP-Botmca9.umx | 357,449 | 63,188,992 |

其他验证：

- 17 / 4096 / 8193 字节容量 PCM 一致，缓冲区哨兵完整。
- 两个 Reader 各完整播放 20 次，第三个 Reader 同时扫描 20 次，输出与独立读取基线逐字节相等。
- 10 组 Seek/Stop/Start 隔离：另一 Reader 读取和定位、第三 Reader 反复打开扫描；目标 Reader 每组 73,728 字节与无干扰基线相等。
- Release 的 XP / Win7 导入审计通过，4 个系统 DLL、78 个导入；继续使用 VC-LTL / YY-Thunks。
- Windows 11 本地验证通过；XP 与 Win7 分别执行 `run.cmd` 和 `run-seek.cmd`，均为 **`MOD_CORE_GROUPS=14 FAILURES=0`**。两个系统同样得到系数及抖动零差异。
- Windows 11、XP、Win7 的原版与重建版真实宿主分别播放 XM、UMX，每系统 4 次、共 12 次通过。GUI 宿主结果与核心探针分别记录。

私有日志：`regression-final.json`、`tables.log`、`seek.log`、`xp-result.log`、`win7-result.log`。

真实宿主日志位于同批私有 `integration/mod-{xp,win7}-result.log` 与 `integration/mod-modern-host-results.json`。发行打包使用上述冻结测试 DLL，SHA256 不变。

### 三组差异的改善与残余

以同一份上轮原 DLL 完整 PCM 为基准，差异值数量包含左右声道：

| 样本 | 修复前不同值 / 最大绝对差 | 修复后不同值 / 最大绝对差 | 修复后平均绝对差 |
|---|---:|---:|---:|
| finefx_ft2.xm | 49,365 / 4,315 | 22 / 54 | 0.00211 |
| ft2_double_toneporta.xm | 169,963 / 4,317 | 1,227 / 56 | 0.04503 |
| ASP-Botmca9.umx | 31,584,305 / 18,270 | 1,492,089 / 2,680 | 1.14571 |

原 DLL 重启进程重复解码时，两个 XM 分别有 22 / 1,306 个值变化。第一组修复后的 22 个差异位置全部在原版自身变化集合中；第二组另有 3 个值位于该集合外，差值均为 1。该观察尚不能证明原版波动的底层原因，不应为了复制不稳定输出而引入未初始化数据。

**真实 UMX 的残余不能归为原版自身波动**：其原版重复输出一致，修复后仍有约 4.72% 的采样值不同，局部最大差为 2,680。通用增益、滤波、抖动与取整已经独立核实；剩余局部混音/循环边界尚未证明完全一致，不声称已解决所有 tracker 效果或所有真实 UMX。

## 冻结产物

- 版本：`2026.10.25`。
- 输出：`ttp_mod/build/Release/ttp_mod.dll`，144,896 字节。
- SHA256：`f84cf526e8201fdef4c78f38fb9d97eee2917f605a551a31c4afad60ea43a6e9`。
- 平铺测试目录：`rebuild/tests/plugins_followup_20261025/mod/vmstage`；`run.cmd` 验证完整 PCM hash、并发、原表/抖动，`run-seek.cmd` 验证隔离。
- `original_mod.dll` 及其 `msvcr110.dll` 仅用于私有原版对照，VC2012 不属于重建 DLL 的运行依赖。
- Windows 11 结果不能当作独立 Windows 10 实测结论。
