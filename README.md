# nput-transcribe

Whisper 语音转写命令行工具（C11）：**编码器跑在 Intel NPU，解码器留在 CPU**。
输入 WAV，以及系统解码器支持的压缩音频（Windows：MP3/M4A/FLAC/WMA）。

```text
nput-transcribe --model <whisper-ir-dir> -f srt -o meeting.srt meeting.mp3
```

> 为什么这样分工：编码器是定长（80 × 3000）的一次性推理，适配 NPU 的静态形状；
> 解码器是自回归的，KV 缓存为动态形状，NPU 编译不了。编码器几乎不占 CPU。

## 快速开始

```powershell
# 1. 构建（GCC + Ninja，产物在 build/debug/bin）
cmake --preset debug
cmake --build --preset debug

# 2. 准备模型（OpenVINO IR）
pip install huggingface_hub openvino
python -c "from huggingface_hub import snapshot_download as d; print(d('OpenVINO/whisper-base-int8-ov', allow_patterns=['*.xml','*.bin','*.json','*.txt']))"

# 3. 转写（自动查找模型目录）
.\build\debug\bin\nput-transcribe.exe -f srt -o meeting.srt meeting.mp3
```

环境：Windows 10/11、GCC（MSYS2 UCRT64，已验证 16.x）、CMake ≥ 3.21、Ninja。
OpenVINO 运行时按 `OPENVINO_ROOT` → 环境变量 → `pip install openvino` 的顺序自动查找；
构建会把约 200 MB 的运行时 DLL 复制到产物目录，程序可脱离 PATH 运行。

模型目录需包含 `openvino_encoder_model.xml/.bin`、`openvino_decoder_model.xml/.bin`
与 `vocab.json`。不传 `--model` 时按 `NPUT_MODEL_DIR` → `./models` → Hugging Face 缓存
的顺序查找，多个候选取最新快照，并在 stderr 打印实际使用的目录。

## 常用命令

| 目的 | 命令 |
| --- | --- |
| 运行测试 | `ctest --preset debug` |
| Release 构建 | `cmake --preset release` + `cmake --build --preset release` |
| 列出设备 | `nput-transcribe --list-devices` |
| 列出可用模型目录 | `nput-transcribe --list-models` |
| 固定中文、输出 SRT | `nput-transcribe -m <模型> -l zh -f srt -o out.srt in.mp3` |
| 自动检测、JSON、看耗时 | `nput-transcribe -m <模型> -f json -v in.wav` |

`--cache-dir` 建议常设：NPU 首次编译约 20–30 s，之后从缓存加载约 1–2 s。
`ctest` 覆盖 WAV、重采样、log-mel（与 transformers 数值一致）、BPE、分段、3-gram 与 CLI。

## 参数

| 参数 | 说明 |
| --- | --- |
| `-m, --model <目录>` | OpenVINO Whisper IR 目录（默认自动查找） |
| `--vocab <文件>` | `vocab.json`，默认 `<model>/vocab.json` |
| `--encoder-device` / `--decoder-device` | 默认 `NPU` / `CPU` |
| `--cache-dir <目录>` | 编译缓存目录 |
| `-l, --language <代码>` | `en`、`zh` 等；`auto` 自动检测（默认） |
| `-f, --format <格式>` | `txt`（默认）、`srt`、`json` |
| `-o, --output <路径>` | 写入文件（自动创建父目录），默认输出到 stdout |
| `--max-tokens <n>` | 每 30 秒片段的 token 上限（默认 224） |
| `-v, --verbose` | 在 stderr 打印每段耗时 |
| `--list-devices` | 列出 OpenVINO 可用设备后退出 |
| `--list-models` | 列出可用模型目录后退出（含 `NPUT_MODEL_DIR`） |
| `--version` / `-h` | 版本 / 帮助 |

需要本机专属构建配置时，把 `CMakeUserPresets.example.json` 复制为
`CMakeUserPresets.json` 再修改（后者已被 `.gitignore` 忽略）。

## 性能实测

Release 构建，whisper-base int8，NPU 编码器：

| 文件 | 时长 | 片段 | 墙钟 | 实时倍率 |
| --- | --- | --- | --- | --- |
| `1.mp3` | 271.2 s | 10 | 7.8 s | 34.7× |
| `2.mp4` | 212.3 s | 8 | 6.6 s | 32.3× |

编码 68–91 ms/段，解码 60–360 ms/段。批量回归（逐文件输出 SRT 与时序汇总）：

```powershell
.\scripts\run-corpus.ps1 -Model <模型> -Media <媒体目录> -Out .\out
```

长音频按能量在静音处切分为不超过 30 s 的片段，SRT 时间轴与输入对齐；
`--encoder-device` 在 NPU 不可用时可改为 `GPU` 或 `CPU`。

## 作为库使用

```cmake
find_package(NpuTranscribe REQUIRED)
target_link_libraries(app PRIVATE NpuTranscribe::nput_core)
```

安装：

```powershell
cmake --preset release
cmake --build --preset release
cmake --install build/release --prefix C:\libs\nput-transcribe
```

消费方配置时把安装目录加入 `CMAKE_PREFIX_PATH`。也可用 `add_subdirectory` /
`FetchContent` 直接接入源码（子项目中 `NPUT_BUILD_CLI`、`NPUT_BUILD_TESTING` 默认关闭）。

- 默认构建静态库；加 `-DBUILD_SHARED_LIBS=ON` 构建共享库，导出宏见
  `include/nput/export.h`。
- `NpuTranscribeConfig.cmake` 会复用同一套 OpenVINO 查找逻辑；运行时仍需能加载
  OpenVINO DLL（把 `%OPENVINO_ROOT%\libs` 加入 PATH，或在 CMake 中调用
  `nput_install_runtime_dlls(app)` 复制到产物目录）。

## VS Code

- CMake Tools 已配置为预设模式：选择 `debug` / `release`，`F7` 构建，Test 按钮跑 ctest。
- `F5` 用 gdb 调试 `build/debug/bin/nput-transcribe.exe`；模型与音频路径启动时输入。
- Debug 构建的频谱计算较慢，调试时建议使用 10 s 以内的音频。

## 目录

```text
include/nput/  公共头文件          src/nput/  实现
src/main.c     CLI                 tests/     Unity 单元测试
cmake/         构建模块            scripts/   构建与语料脚本
```

## 已知限制

- 仅贪心解码（3-gram 去重），没有束搜索、温度回退和时间戳 token。
- 中文输出可能为繁体；对演唱/纯音乐识别质量有限（模型能力，已用 `openvino_genai`
  交叉验证为同一结果）。
- 音频整体载入内存：1 小时 44.1 kHz 立体声峰值约 1 GB。
- 非 Windows 平台只能处理 WAV；Media Foundation 固定要求 16 位 PCM。

## 许可证

MIT，详见 [LICENSE](LICENSE)。

---

贡献规范见 [CONTRIBUTING.md](CONTRIBUTING.md)，安全问题按 [SECURITY.md](SECURITY.md) 报告，
版本变更见 [CHANGELOG.md](CHANGELOG.md)。

