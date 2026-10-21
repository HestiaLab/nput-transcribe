# npu-transcribe 架构模型摘要

- 范围：仓库当前状态（v0.3.1，`main`），只描述已实现的架构，不含目标态设计。
- 方法：以代码、CMake 配置与仓库文档为证据，证据索引见
  [system-model.evidence.md](system-model.evidence.md)。
- 本次未重新测量性能、未验证 NPU 驱动状态，也未执行真实音频推理。

## 1. 这套模型回答什么

| 问题 | 视图 |
| --- | --- |
| 系统边界是什么？谁在使用？依赖哪些外部系统？ | C4 `SystemContext` |
| 有哪些可部署/可复用单元和数据存储？ | C4 `Containers` |
| CLI 与 `nput_core` 内部如何分工？ | C4 `CliComponents`、`CoreComponents` |
| 一段音频如何变成文本？设备边界在哪？ | [推理流水线 DOT](npu-transcribe-inference-flow.dot) |
| 子系统之间谁依赖谁？公共契约在哪？ | [模块依赖 DOT](npu-transcribe-modules.dot) |

推荐阅读顺序：系统上下文 → 容器 → 推理流水线 → 模块依赖 → 组件视图。
只关心代码结构时可从模块依赖开始；只关心运行行为时可直接看流水线。

## 2. 关键架构结论

1. **双入口、单一实现**：`nput-transcribe` CLI 与外部集成者都通过
   `include/nput` 公共头使用能力；实现集中在 `nput_core`，OpenVINO 以
   `PRIVATE` 方式链接，不泄漏到公共接口。
2. **设备分工由形状约束决定**：编码器输入固定为 `1×80×3000`，输出
   `1×1500×d_model`，适合 NPU 静态形状；解码器自回归、KV 缓存为动态形状，
   留在 CPU。默认设备即 `NPU` 编码器 + `CPU` 解码器。
3. **单文件流水线**：加载 → WAV 解析或 Media Foundation 解码 → 窗函数 sinc
   重采样到 16 kHz 单声道 → 能量分段（≤30 s）→ log-mel `80×3000` →
   NPU 编码器 → CPU 贪心解码（SOT、语言、transcribe、no_timestamps、
   3-gram 抑制、`max_tokens` 默认 224）→ 字节级 BPE 解码 → 去空白 →
   `txt` / `srt` / `json`。
4. **自动语言检测有额外成本**：每个文件只在首个片段检测一次语言；检测与
   该片段的转写各自重算一次 log-mel 并执行一次编码器。后续片段复用该语言
   token。这是当前流水线中最明确的重复计算点。
5. **后端是可替换接缝**：`nput_asr_backend` 暴露四个函数指针
   （`create` / `detect_language` / `transcribe` / `destroy`），会话层按名称
   分派；当前只内置 Whisper，测试通过注册桩后端覆盖会话行为。
6. **平台差异被收敛**：`#ifdef` 集中在 `platform.c` 与
   `media_foundation.c`；非 Windows 平台只支持 WAV，压缩音频解码返回
   `NPUT_ERR_UNSUPPORTED`。
7. **文件型数据面**：Whisper IR 目录、`vocab.json`、OpenVINO 编译缓存
   （`--cache-dir`）与输入音频是主要外部数据；模型发现同时扫描
   `./models` 与 Hugging Face 缓存。
8. **交付形态**：`nput_core` 默认静态库、可选共享库，支持
   `find_package(NpuTranscribe)`；测试覆盖 11 个 Unity 目标、公共 API
   表面检查与 CLI 冒烟；CI 覆盖 Windows MinGW 与 Linux GCC/Clang/ASan；
   Release 产出带 OpenVINO DLL 的 Windows 便携 zip。

## 3. 视图说明

### C4 模型

[npu-transcribe.structurizr.dsl](npu-transcribe.structurizr.dsl) 是 C4 的规范源，
包含四个视图：`SystemContext`、`Containers`、`CliComponents`、`CoreComponents`。
模型使用 `!identifiers hierarchical`，关系与视图均使用完整点路径。

### 推理流水线

[npu-transcribe-inference-flow.dot](npu-transcribe-inference-flow.dot) 按运行阶段
分组：输入、CPU 音频前端、CPU 特征提取、NPU 编码器、CPU 解码器、输出、
模型与缓存。实线为主数据流，虚线为可选/按需路径，点线为计时观测旁路。
图中同时标出了自动语言检测的重复计算路径。

### 模块依赖

[npu-transcribe-modules.dot](npu-transcribe-modules.dot) 是子系统级视图，而不是
文件级依赖图：CLI 只依赖公共契约；`nput_core` 以一条簇边界边表示“实现公共
契约”；内部按 `asr`、`audio`、`text`、`platform`、`status` 分组，外部依赖为
OpenVINO 与 Windows Media Foundation。文件级细节保留在 C4 组件视图与证据
索引中。

## 4. 证据强度与未知项

大部分结构、依赖与行为结论为 `high`（代码或配置直接确认）。以下结论较弱或
未验证：

| 项 | 状态 | 影响 |
| --- | --- | --- |
| 解码器 IR 的 KV 缓存内部实现 | unknown | 决定设备分配能否调整；仓库内无 IR |
| 本机 NPU 可用性与驱动/固件 | unknown | 编码器默认设备不可用时会失败或需改设备 |
| README 性能数字（34.7× / 32.3×） | 未复测 | 依赖硬件、模型与音频 |
| Linux 上的真实推理 | unknown | CI 只构建与跑测试 |
| 会话并发约束 | medium | 头文件声明单会话不可并发，未做并发测试 |
| `large-v3`（128 mel） | 不支持 | 当前固定 80 mel bins |

## 5. 产物与验证状态

| 产物 | 角色 | 状态 |
| --- | --- | --- |
| `npu-transcribe.structurizr.dsl` | C4 规范源 | 已生成，未用 Structurizr 解析器校验（本机无 CLI/Java） |
| `npu-transcribe-inference-flow.dot` | 流水线规范源 | 已生成 |
| `npu-transcribe-inference-flow.svg/.png` | 流水线派生预览 | 已通过 `@viz-js/viz` + `@resvg/resvg-js` 渲染并目视检查 |
| `npu-transcribe-modules.dot` | 模块依赖规范源 | 已生成 |
| `npu-transcribe-modules.svg/.png` | 模块依赖派生预览 | 已渲染并目视检查；PNG 因回退字体有轻微文字溢出，SVG 为准 |
| `system-model.evidence.md` | 节点/边/行为证据索引 | 已完成 |
| `system-model.summary.md` | 本摘要 | 已完成 |

验证口径：

- **已通过**：两张 DOT 图均成功渲染为 SVG/PNG，并人工检查了主路径、重复
  计算旁路与模块依赖的可读性。
- **未执行**：Structurizr DSL 解析；`cmake --build`、`ctest`、真实音频推理。
  本次是纯文档新增，未改动源码、CMake 或测试。
- **无法执行**：用 Structurizr CLI/Lite 打开 DSL —— 本机无 Java 与
  Structurizr CLI。可在装有 Structurizr Lite 的环境中直接导入该 `.dsl`
  文件验证。

DOT 是规范源，SVG/PNG 为派生文件；修改架构时先改 DOT，再重新渲染。C4 模型
可在 Qoder 中用 Structurizr DSL 查看器打开；静态预览直接查看 SVG。
