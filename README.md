# TTPlayer Modules 插件重建

恢复原版一个 `Modules Reader` 工厂，扩展名保持 `mod;far;it;s3m;stm;mtm;umx;xm`。固定输出 44100 Hz／16-bit／双声道，提供只读标题、格式、时长、定位和读取。

默认采用原 ModPlug 谱系的 **libmodplug 0.8.9.0**。八类样本的音序时长和播放结束输出长度均与原 DLL 一致；修复毫秒精度、IStream／Unicode 输入、短读、容量对齐及并发读取。现代 libopenmpt 保留为显式构建选项。

## 默认兼容构建

```powershell
./build.ps1 -Package
```

固定来源：https://downloads.sourceforge.net/project/modplug-xmms/libmodplug/0.8.9.0/libmodplug-0.8.9.0.tar.gz

SHA-256：`457ca5a6c179656d66c01505c0d95fafaead4329b9dbaa0f997d00a3508ad9de`。构建下载校验，不提交上游源码。public-domain 声明、现代可选核及兼容依赖的许可证保存在 `licenses/`。

现代 MSVC、Win32、VC-LTL／YY-Thunks，XP SP3／Win7／新系统共用，要求 SSE2。Action 独立构建北京时间日期版本，运行包仅含 DLL 与 SHA256SUMS。私有测试不上传、不参与 Action。

## 显式现代内核

```powershell
./build.ps1 -BuildDirectory build-modern -CMakeArguments '-DTTP_MOD_MODERN_ENGINE=ON' -Package
```

固定 libopenmpt **0.8.9**，来源及 SHA-256 见 `cmake/modern_codec.cmake`。此模式采用现代 Tracker 的效果解释、混音、定位和曲尾规则，可能改变历史曲目的音序与时长；它不是默认兼容模式，也不会自动作为读取回退。使用固定种子的原 ModPlug 0.5-bit 抖动。

输入直接读取宿主 IStream，支持 Unicode、压缩包虚拟流、短读和不可定位输入；载入后内存定位，一次输入上限 256 MiB。证据、并发隔离和边界见 `docs/RECONSTRUCTION.md`。

## 可选格式识别接口

2026-10-20：Reader 工厂增加 Probe，Reader 增加 SourceInfo；旧导出和接口槽位保持不变。声明与辅助实现位于 `include/ttpcomm/audio_format_probe.h` 和 `audio_format_probe_impl.h`，不增加运行时 DLL 依赖。协议原件在相邻 `ttpcomm/sdk/AUDIO_FORMAT_PROBE.md`；新建插件指南在相邻 `rebuild/docs/AUDIO_PLUGIN_DEVELOPMENT_GUIDE.md`。原版播放器继续使用旧接口；错误后缀候选选择和更名提示由支持新协议的重建播放器执行。
