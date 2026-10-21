# nput-transcribe 架构证据索引

- 范围：仓库 `HestiaLab/npu-transcribe` 的当前状态（v0.3.1，分支 `main`）。
- 证据类型：code / config / document。运行时设备状态与性能数字未在本任务中重新测量。
- 置信度：`high` = 代码或配置直接确认；`medium` = 文档陈述或多个间接信号一致；`low` = 推断；`unknown` = 未验证。

## 1. 系统与容器节点

| 节点 ID | 节点 | 层级 | sourceRefs | 置信度 |
| --- | --- | --- | --- | --- |
| `nput` | nput-transcribe 系统 | L1 | `README.md:5-15`, `CMakeLists.txt:3-7` | high |
| `nput.cli` | nput-transcribe CLI 容器 | L2 | `CMakeLists.txt:75-87`, `cmake/Sources.cmake:19-23`, `src/main.c:461-598` | high |
| `nput.core` | nput_core 库容器 | L2 | `CMakeLists.txt:38-73`, `cmake/Sources.cmake:3-17` | high |
| `nput.models` | Whisper OpenVINO IR 目录 | L2（数据） | `README.md:36-38`, `src/nput/asr/backend_whisper.c:116-139` | high |
| `nput.compileCache` | OpenVINO 编译缓存 | L2（数据） | `src/nput/asr/backend_whisper.c:170-177`, `README.md:55` | high |
| `nput.media` | 输入音频文件 | L2（数据） | `README.md:7-8`, `src/nput/audio/audio.c:49-57` | high |
| `openvino` | OpenVINO Runtime | 外部系统 | `cmake/OpenVINO.cmake:1-109`, `src/nput/asr/backend_whisper.c:3` | high |
| `mediaFoundationSystem` | Windows Media Foundation | 外部系统 | `CMakeLists.txt:63-66`, `src/nput/platform/media_foundation.c:7-13` | high |
| `modelSource` | Hugging Face 缓存 / 本地模型源 | 外部系统 | `src/nput/platform/platform.c:172-225`, `README.md:37-38` | high |
| `operator` | 命令行用户 | 角色 | `README.md:10-13`, `src/main.c:111-134` | high |
| `integrator` | 库集成开发者 | 角色 | `README.md:148-170`, `include/nput/asr.h:21-109` | high |

## 2. 组件节点（CLI）

| 节点 ID | 组件 | sourceRefs | 置信度 |
| --- | --- | --- | --- |
| `nput.cli.cliEntry` | CLI 入口与编排 | `src/main.c:461-598`, `src/main.c:362-459` | high |
| `nput.cli.cliArgs` | 参数解析辅助 | `src/main.c:143-327`, `src/cli_args.c:1-18` | high |
| `nput.cli.cliOutput` | 结果格式化 | `src/cli_output.c:90-140` | high |

## 3. 组件节点（nput_core）

| 节点 ID | 组件 | sourceRefs | 置信度 |
| --- | --- | --- | --- |
| `nput.core.asrSession` | ASR 会话层 | `src/nput/asr/asr.c:17-149` | high |
| `nput.core.backendContract` | ASR 后端接口（扩展点） | `src/nput/asr/backend.h:22-31`, `CONTRIBUTING.md:40-53` | high |
| `nput.core.whisperBackend` | Whisper 后端 | `src/nput/asr/backend_whisper.c:99-604` | high |
| `nput.core.ovDevices` | OpenVINO 设备枚举 | `src/nput/asr/ov_runtime.c:8-47` | high |
| `nput.core.audioLoad` | 音频加载分发 | `src/nput/audio/audio.c:49-57` | high |
| `nput.core.wavDecoder` | WAV 解析 | `src/nput/audio/wav.c:93-244` | high |
| `nput.core.mediaFoundation` | Media Foundation 解码 | `src/nput/platform/media_foundation.c:40-210` | high |
| `nput.core.resampler` | 重采样 | `src/nput/audio/resample.c:28-89` | high |
| `nput.core.segmenter` | 能量分段 | `src/nput/audio/segment.c:13-95` | high |
| `nput.core.mel` | log-mel 特征 | `src/nput/audio/mel.c:94-185`, `include/nput/mel.h:13-20` | high |
| `nput.core.tokenizer` | 字节级 BPE 解码 | `src/nput/text/tokenizer.c:220-381` | high |
| `nput.core.trigram` | 3-gram 重复抑制 | `src/nput/text/trigram.h:15-31` | high |
| `nput.core.platform` | 平台文件与模型发现 | `src/nput/platform/platform.c:18-85`, `src/nput/platform/platform.c:106-235`, `src/nput/platform/platform.c:261-329` | high |
| `nput.core.clock` | 单调时钟 | `src/nput/platform/clock.c:14-27` | high |
| `nput.core.status` | 状态码与错误详情 | `src/nput/core/status.c:16-54`, `include/nput/status.h:10-45` | high |

## 4. 关键关系

| 关系 | 类型 | sourceRefs | 置信度 |
| --- | --- | --- | --- |
| CLI → nput_core | 链接 / C ABI | `CMakeLists.txt:75-77`, `README.md:95` | high |
| nput_core → OpenVINO Runtime | 运行时调用 | `CMakeLists.txt:59`, `src/nput/asr/backend_whisper.c:141-202` | high |
| nput_core → Media Foundation | 运行时调用 | `CMakeLists.txt:63-66`, `src/nput/platform/media_foundation.c:51-196` | high |
| nput_core → 模型目录 | 文件读取 | `src/nput/asr/backend_whisper.c:116-139`, `src/nput/asr/backend_whisper.c:204-209` | high |
| nput_core → 编译缓存 | 文件读写 | `src/nput/asr/backend_whisper.c:170-177` | high |
| 模型发现 → Hugging Face 缓存 | 文件系统扫描 | `src/nput/platform/platform.c:172-225`, `src/nput/platform/platform.c:433-499` | high |
| CLI 编排 → 音频加载 | 公共 API 调用 | `src/main.c:367-371`, `include/nput/audio.h:40-49` | high |
| CLI 编排 → 重采样 | 公共 API 调用 | `src/main.c:379-385`, `include/nput/mel.h:14` | high |
| CLI 编排 → 分段 | 公共 API 调用 | `src/main.c:391-399`, `include/nput/segment.h:33-35` | high |
| CLI 编排 → ASR 会话 | 公共 API 调用 | `src/main.c:410-421`, `include/nput/asr.h:89-102` | high |
| ASR 会话 → 后端接口 | 运行时分派 | `src/nput/asr/asr.c:34-61`, `src/nput/asr/asr.c:94-110` | high |
| Whisper 后端 → 后端接口 | 接口实现 | `src/nput/asr/backend_whisper.c:606-608` | high |
| Whisper 后端 → log-mel | 函数调用 | `src/nput/asr/backend_whisper.c:518-527`, `src/nput/asr/backend_whisper.c:552-561` | high |
| Whisper 后端 → BPE 解码 | 函数调用 | `src/nput/asr/backend_whisper.c:582-599` | high |
| Whisper 后端 → 3-gram 抑制 | 头文件内联 | `src/nput/asr/backend_whisper.c:440-477`, `src/nput/text/trigram.h:15-31` | high |
| Whisper 后端 → 单调时钟 | 函数调用 | `src/nput/asr/backend_whisper.c:256-261`, `src/nput/asr/backend_whisper.c:573-579` | high |

## 5. 关键行为证据

| 行为 | 说明 | sourceRefs | 置信度 |
| --- | --- | --- | --- |
| 编码器静态形状 | 加载后把编码器输入 reshape 为 `1×80×3000` | `src/nput/asr/backend_whisper.c:152-169` | high |
| 默认设备 | 编码器 `NPU`，解码器 `CPU` | `src/nput/asr/asr.c:14-15`, `src/nput/asr/asr.c:64-71` | high |
| 分段上限 | 每段最多 30 秒，在上限前最后 2 秒内找最安静 20 ms 帧 | `src/main.c:393-394`, `src/nput/audio/segment.c:7-11`, `src/nput/audio/segment.c:42-69` | high |
| 自动语言检测 | 每个文件首个片段检测；检测与转写各自计算一次 log-mel 并执行一次编码器 | `src/main.c:410-421`, `src/main.c:552-555`, `src/nput/asr/backend_whisper.c:518-534`, `src/nput/asr/backend_whisper.c:552-575` | high |
| 贪心解码 | 逐 token argmax；无束搜索/温度回退 | `src/nput/asr/backend_whisper.c:432-492`, `README.md:215-218` | high |
| 3-gram 抑制 | 已出现的 3-gram 候选会被替换为次高分候选 | `src/nput/asr/backend_whisper.c:440-477` | high |
| 输出格式 | txt / srt / json；SRT 时间轴来自片段起止 | `src/cli_output.c:90-140`, `src/main.c:431-435` | high |
| 库错误契约 | 库不写 stderr；错误详情经 `nput_last_error()` 读取 | `README.md:111-112`, `src/nput/core/status.c:41-49` | high |
| 平台差异 | 非 Windows 仅 WAV；`nput_platform_decode` 返回 `NPUT_ERR_UNSUPPORTED` | `src/nput/platform/media_foundation.c:212-220`, `README.md:221` | high |
| 会话线程模型 | 单会话不可并发；会话之间可并发 | `include/nput/asr.h:14-20` | medium（头文件声明，未做并发测试） |
| KV 缓存动态形状 | 解码器 IR 的 KV 缓存为动态形状，因此不放到 NPU | `README.md:14-15` | medium（模型 IR 不在仓库内，未直接检查） |

## 6. 构建、验证与交付证据

| 事项 | sourceRefs | 置信度 |
| --- | --- | --- |
| `nput_core` 目标与 OpenVINO 私有链接 | `CMakeLists.txt:38-73` | high |
| CLI 目标与 Windows `shell32` 链接 | `CMakeLists.txt:75-87` | high |
| 安装 / `find_package(NpuTranscribe)` 导出 | `CMakeLists.txt:94-124`, `cmake/NpuTranscribeConfig.cmake.in` | high |
| 单元测试与策略检查 | `cmake/Tests.cmake:34-100` | high |
| CI：Windows MinGW 与 Linux GCC/Clang/ASan | `.github/workflows/ci.yml:23-115` | high |
| Release：Windows 便携 zip + GitHub Release | `.github/workflows/release.yml:17-84`, `scripts/package.ps1:1-20` | high |
| 真实语料回归脚本 | `scripts/run-corpus.ps1`, `AGENTS.md` 验证要求 | high |

## 7. 假设与未知项

| 项 | 状态 | 为什么重要 | 验证方式 |
| --- | --- | --- | --- |
| 解码器 IR 内部 KV 缓存实现 | unknown | 决定能否改变设备分配或批处理 | 检查 `openvino_decoder_model.xml`，或用 `--decoder-device CPU/GPU` 对比 |
| 本机 NPU 可用性与驱动/固件版本 | unknown | 编码器默认设备是 NPU；不可用时需改 GPU/CPU | `nput-transcribe --list-devices`；`scripts/package.ps1 -VerifyDevices` |
| README 性能数字（34.7× / 32.3×） | 未复测 | 依赖具体硬件、模型与音频 | 用真实音频运行 `nput-transcribe -v` 或 `scripts/run-corpus.ps1` |
| Linux 上的真实推理 | unknown | CI 只构建和跑测试，不执行真实模型 | 在 Linux + OpenVINO 环境跑真实音频 |
| 其他 ASR 后端 | 未实现 | 架构保留了 `nput_asr_backend` 扩展点 | 按 `CONTRIBUTING.md:40-53` 新增后端与桩测试 |
| 并发使用 | 未验证 | 头文件声明单会话不可并发 | 增加并发测试或明确调用约束 |
| 大模型 `large-v3`（128 mel） | 不支持 | 当前固定 80 mel bins | `README.md:40-42` |
