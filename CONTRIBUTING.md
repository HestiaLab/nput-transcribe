# 贡献指南

## 开始之前

- 仓库面向中文开发者：文档、代码注释与 CLI 文案统一使用中文，标识符保持英文。
- 构建与测试使用 CMake Presets（GCC + Ninja）：

```powershell
cmake --preset debug
cmake --build --preset debug
ctest --preset debug
```

- 改动推理路径后，再用一段真实音频确认转写与耗时：

```powershell
.\build\debug\bin\nput-transcribe.exe -m <模型目录> -v <音频文件>
```

## 改动流程

1. 先写验收：说明要改变的行为，以及验证用的命令与预期结果。
2. 跨文件改动或修改接口 / 构建配置时，先给出方案（范围、兼容策略、分步计划）。
3. 保持改动聚焦：不顺手重构、不格式化无关代码。
4. 提交前本地验证；涉及构建配置时，`debug` 与 `release` 两个预设都要构建并跑测试。

## 新增 ASR 后端

1. 在 `src/nput/backend_<名称>.c` 中实现 `nput_asr_backend` 的五个成员：
   - `create` 不得保存 `nput_asr_options` 中的字符串指针；
   - `transcribe` 分配文本并填充 `nput_asr_timings`；
   - `detect_language` 不更新 `nput_asr_last_timings`；
   - 模型专属常量（张量名、静态形状、特殊 token、语言码表、词表边界）留在该文件。
2. 在 `src/nput/asr.c` 的 `backends[]` 中注册为内置后端；仅测试使用的后端可在
   测试代码中调用 `nput_asr_backend_register`。
3. 把新文件加入 `cmake/Sources.cmake` 的 `NPUT_CORE_SOURCES`。
4. 按 `tests/test_asr_backend.c` 的模式增加不依赖真实模型的测试，覆盖后端选择、
   转写、语言检测与错误路径；测试必须能在无 NPU、无模型文件的机器上通过。
5. 公共头文件只暴露 `include/nput/` 中的类型；调用方只传 16 kHz 单声道采样点，
   不感知特征表示。默认后端与 CLI 行为不得改变。

## 分支与提交

分支命名：

```text
feat/<主题>    fix/<主题>    docs/<主题>
```

提交信息使用 Conventional Commits：

```text
<type>(<scope>): <简述>

<正文：动机、影响范围、验证方式>
```

- `type`：`feat`、`fix`、`refactor`、`perf`、`test`、`docs`、`build`、`chore`。
- `scope`：`cli`、`asr`、`mel`、`wav`、`build`、`docs` 等。

## Pull Request

- [ ] 说明目标与验收命令，附实际输出。
- [ ] GitHub Actions 的 CI 通过（Windows MinGW GCC、Linux GCC / Clang、ASan）。
- [ ] `ctest --preset debug` 全部通过；改推理路径时附真实音频的 `-v` 结果。
- [ ] 公开接口只放在 `include/nput/`，实现细节留在 `src/`。
- [ ] 设备相关的 `#ifdef` 收敛在 `src/nput/platform.c` 与 `src/nput/media_foundation.c`。
- [ ] 不为通过测试放宽断言或阈值（`tests/test_mel.c` 的参考值来自 transformers）。
- [ ] 公共 API 变更说明影响范围，并按 SemVer 更新版本号与 `CHANGELOG.md` 迁移说明。
- [ ] 新增或修改后端时，桩测试覆盖选择、转写、语言检测与错误路径。
- [ ] 面向用户的变更同步更新 `README.md` 与 `CHANGELOG.md`。
- [ ] 不包含无关改动、调试输出或临时文件。

## 代码风格

- C11；`clang-format` 与 `clang-tidy` 配置在仓库根目录。
- 注意数据所有权：返回 `char **` 的内存由调用方用 `nput_free` 释放，其余在头文件中注明。
- 注释解释“为什么”，不复述代码；公共函数在 `include/nput/` 中保持简短说明。

## 报告问题

- 缺陷与功能建议：GitHub Issues。
- 安全漏洞：按 [SECURITY.md](SECURITY.md) 私下报告，不要创建公开 issue。
