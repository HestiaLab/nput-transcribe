# 变更日志

格式参考 [Keep a Changelog](https://keepachangelog.com/zh-CN/1.1.0/)，
版本号遵循 [语义化版本](https://semver.org/lang/zh-CN/)。

## [Unreleased]

### 新增

- `-o` 指定的输出路径会自动创建不存在的父目录。
- `--model` 可省略：按 `NPUT_MODEL_DIR` → `./models` → Hugging Face 缓存自动查找，
  多个候选取最新快照，并打印实际使用的目录。
- 新增 `--list-models`：列出自动查找可用的模型目录（含 `NPUT_MODEL_DIR`）。
- 安装与包支持：`cmake --install` 后可用 `find_package(NpuTranscribe)` 并链接
  `NpuTranscribe::nput_core`；支持 `BUILD_SHARED_LIBS` 构建共享库与 `NPUT_API` 导出宏。

## [0.1.0] - 2026-10-19

首个版本。

### 新增

- Whisper 转写：编码器运行在 Intel NPU（输入固定 `1 x 80 x 3000`），解码器留在 CPU。
- 音频输入：WAV 解析（8/16/24/32 位整数、32/64 位浮点、多声道混为单声道）；
  Windows 上通过 Media Foundation 解码 MP3/M4A/FLAC/WMA。
- 处理链：重采样到 16 kHz → 30 秒分段（静音处切分）→ log-mel（与 transformers
  `WhisperFeatureExtractor` 数值一致）→ 贪心解码（3-gram 去重）。
- CLI：`txt`/`srt`/`json` 输出，语言自动检测或指定，`--list-devices`、
  `--cache-dir` 编译缓存、`--verbose` 分段耗时。
- 构建：CMake Presets（`debug` / `release`）+ Ninja + GCC；自动定位 OpenVINO
  运行时并复制 DLL。
- 开发环境：VS Code 预设构建与 gdb 调试配置，`scripts/build.ps1`、
  `scripts/run-corpus.ps1`。
- 测试：Unity 单元测试覆盖 WAV、重采样、log-mel、BPE、分段与 3-gram，
  以及 CLI 的 `--help` / `--version` 冒烟测试。
