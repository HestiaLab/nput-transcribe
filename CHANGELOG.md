# 变更日志

格式参考 [Keep a Changelog](https://keepachangelog.com/zh-CN/1.1.0/)，
版本号遵循 [语义化版本](https://semver.org/lang/zh-CN/)。

## [Unreleased]

### 新增

- GitHub Actions CI：Windows（MinGW GCC）与 Linux（GCC、Clang、Debug + ASan）构建并运行 `ctest`。
- 标签触发的 Release 工作流：构建、测试、打包 Windows x64 产物并创建 GitHub Release。

### 修复

- Linux 构建：`platform.c` 与 `wav.c` 声明 POSIX 特性宏，修复 `time_t`、`mkdir`、
  `fseeko` 等符号不可见的问题。
- `cmake/OpenVINO.cmake` 支持 Linux wheel 中带版本号的 `libopenvino_c.so`。

## [0.2.0] - 2026-10-20

### 变更

- 公共 ASR 接口与 Whisper 解耦：`nput_asr_transcribe` / `nput_asr_detect_language`
  改为直接接收 16 kHz 单声道采样点（`const float *samples, size_t count`），
  调用方不再需要构造 `nput_mel`。
- `nput_asr_options` 新增 `backend` 字段；`NULL`、`""` 或 `"whisper"` 使用 Whisper，
  默认行为不变。CLI 参数未改变。
- Whisper 实现迁入 `src/nput/backend_whisper.c`：特殊 token、张量名、静态形状、
  语言码表与词表上界集中在该文件；`src/nput/asr.c` 只负责后端选择与分发。

### 新增

- 内部后端表（create / detect_language / transcribe / destroy），为接入 Paraformer、
  SenseVoice 等其它 ASR 模型提供扩展点。
- 内部后端注册接口 `nput_asr_backend_register`：仅供测试使用，不在安装头文件中，
  不属于稳定公共 API。
- `nput_tokenizer_decode_below` 内部接口，允许后端指定自己的特殊 token 边界。
- 测试：桩后端覆盖后端选择、转写、语言检测、未知后端与参数错误；ctest 由 8 项增至 9 项。

### 破坏性变更

- `nput_asr_transcribe` / `nput_asr_detect_language` 的签名改变，
  `nput_asr_options` 尺寸改变；需要重新编译调用方。
- `nput_mel` 仍是公共能力（`nput_mel_compute` 可单独使用），但不再是 ASR 接口的输入。

### 迁移

```c
/* 0.1.x */
nput_mel mel;
nput_mel_compute(samples, count, &mel);
nput_asr_transcribe(asr, &mel, language, &text);
nput_mel_free(&mel);

/* 0.2.0：samples 为 16 kHz 单声道，count 通常不超过 480000 */
nput_asr_transcribe(asr, samples, count, language, &text);
nput_asr_detect_language(asr, samples, count, &language);
```

## [0.1.1] - 2026-10-19

### 新增

- 打包：`scripts/package.ps1` 生成 Windows 便携包（exe、OpenVINO DLL、许可证、
  第三方声明与 `SHA256SUMS.txt`）；新增 `THIRD_PARTY_NOTICES.md` 与
  `LICENSES/Apache-2.0.txt`。

## [0.1.0] - 2026-10-19

首个版本。

### 新增

- Whisper 转写：编码器运行在 Intel NPU（输入固定 `1 x 80 x 3000`），解码器留在 CPU。
- 音频输入：WAV 解析（8/16/24/32 位整数、32/64 位浮点、多声道混为单声道）；
  Windows 上通过 Media Foundation 解码 MP3/M4A/FLAC/WMA。
- 处理链：重采样到 16 kHz → 30 秒分段（静音处切分）→ log-mel（与 transformers
  `WhisperFeatureExtractor` 数值一致）→ 贪心解码（3-gram 去重）。
- 模型目录：`--model` 可省略，按 `NPUT_MODEL_DIR` → `./models` → Hugging Face 缓存
  自动查找，多个候选取最新快照，并在 stderr 打印实际使用的目录。
- CLI：`txt`/`srt`/`json` 输出，语言自动检测或指定，`--list-devices`、`--list-models`、
  `--cache-dir` 编译缓存、`--verbose` 分段耗时；`-o` 自动创建父目录。
- 库接口：C11 公共 API（`include/nput`）；`cmake --install` 后可用
  `find_package(NpuTranscribe)` 链接 `NpuTranscribe::nput_core`，支持
  `BUILD_SHARED_LIBS` 共享库与 `NPUT_API` 导出宏。
- 构建：CMake Presets（`debug` / `release`）+ Ninja + GCC；自动定位 OpenVINO
  运行时并复制 DLL。
- 开发环境：VS Code 预设构建与 gdb 调试配置，`scripts/build.ps1`、
  `scripts/run-corpus.ps1`。
- 测试：Unity 单元测试覆盖 WAV、重采样、log-mel、BPE、分段与 3-gram，
  以及 CLI 的 `--help` / `--version` 冒烟测试。
