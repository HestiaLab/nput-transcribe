# AGENTS.md

面向在本仓库工作的 AI 代理的说明。

## 项目形状

C11 + CMake 的控制台程序，把 Whisper 编码器放到 Intel NPU、解码器留在 CPU。

```text
include/nput/   公共 API
src/nput/       实现
src/main.c      CLI
tests/          Unity 单元测试
cmake/          构建模块
```

## 构建与验证

```powershell
cmake --preset debug
cmake --build --preset debug
ctest --preset debug
```

`nput_core` 依赖 OpenVINO C API；`cmake/OpenVINO.cmake` 会从 `OPENVINO_ROOT` 或 Python
包目录定位它。测试不需要 NPU，但 `nput-transcribe` 运行真实推理时需要。

## 约束

- 公共接口只在 `include/nput/`，实现细节留在 `src/`。
- 不为了让测试通过而放宽断言或阈值。`test_mel.c` 的参考值来自 transformers 的
  `WhisperFeatureExtractor`，属于独立来源，不要用本仓库的输出反向生成。
- NPU 只接受静态形状：编码器输入固定为 `1 x 80 x 3000`，不要把它改成动态。
- 不要在 NPU 上编译解码器：该 IR 的 KV 缓存是动态形状，只会失败。
- 新增设备相关代码时，把 `#ifdef` 收敛在 `src/nput/platform.c` 与
  `src/nput/media_foundation.c`。

## 验证要求

改动音频或频谱相关代码后，至少运行 `ctest`；改动推理路径后，再用一段真实音频跑
`nput-transcribe -v`，确认编码器与解码器耗时合理且转写内容正确。

真实语料回归：

```powershell
.\scripts\run-corpus.ps1 -Model <whisper-ir> -Media <media-dir> -Out .\out
```

该脚本要求每个文件 `ExitCode` 为 0、`RealtimeX` 明显大于 1，并且不出现跑满
`--max-tokens` 的退化片段（除静音/音乐片段外）。

## 协作协议

本仓库按风险校准协作执行，只保留以下规则。

### 1. 先写验收，再动手

开工前用两句话说明：

```text
目标：要改变什么行为
验收：跑什么命令、看到什么结果
```

跨文件改动或改接口/配置时，另加 `保留`（哪些行为不得变）与 `禁区`（不得触碰的范围）。

**写不出验收 = 任务尚未定义清楚，先澄清，不动手。**

### 2. 按风险决定流程重量

| 级别 | 判定 | 流程 |
|---|---|---|
| S | 单文件、可一步撤回、有测试可验证 | 最小改动 → 运行验收 → 复核 diff |
| M | 跨文件，或改接口/数据结构/CMake 配置，或无测试覆盖 | **方案先行**：先给范围、兼容策略、分步计划，确认后动手 |
| L | 删除/迁移数据、改认证密钥、外部交付、不可回滚 | M + 显式授权（影响范围 / 失败后果 / 恢复路径）+ 恢复预案 |

硬规则（优先于判断）：改 `CMakeLists.txt`、`cmake/`、依赖版本 → ≥ M；无版本控制或工作区不可回滚 → ≥ M 且禁止不可逆操作；改动他人未提交的变更 → 停止确认。

不确定时升一级，**但只升一级**；过重的流程会被绕过，等于没有流程。

### 3. 报告完成必须给证据

只允许五种状态：**已通过 / 已失败 / 未执行 / 无法执行 / 结果不确定**。禁止"应该没问题""看起来对"。

完成声明必须写成 **E4(C)**：实际执行并通过、且覆盖声明范围 C 的命令输出，并列出未覆盖的部分。

- 编译通过是 E2，不是完成依据；`ctest` 全绿只覆盖被测路径，CLI、stdin、错误分支不因此获得证据。
- 同一产物对不同 C 的证据等级不同。例如 `ctest` 通过是 E4(C=单元测试覆盖的模块)，对 `nput-transcribe -v` 的端到端行为只是 E3。

### 确认门

删除或覆写文件、改 `.git` 历史、强制推送、触碰未声明模块、引入新依赖，必须停下确认，并给出影响范围、失败后果、恢复路径。**沉默不等于批准。**

禁止顺手重构、格式化或补注释；发现相邻问题只记录，不在本次处理。

