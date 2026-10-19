# 安全策略

## 支持版本

| 版本 | 支持状态 |
| --- | --- |
| 0.1.x | 接收安全报告 |

## 报告漏洞

请通过仓库的 **Security → Report a vulnerability**（GitHub Security Advisories）
私下报告，不要创建公开 issue。

报告请包含：

- 受影响版本与平台；
- 复现步骤或最小样例；
- 影响面（崩溃、越界、信息泄露等）；
- 建议修复（如有）。

收到报告后会尽快确认；修复发布后再公开说明（如你同意，会致谢报告者）。

## 威胁模型

本项目是本地命令行工具，不监听端口、不发起网络请求。主要风险面：

- **不可信媒体文件**：WAV 解析与 Media Foundation 解码在进程内执行，损坏或恶意文件
  可能触发解析缺陷。只处理可信来源的文件，或先在沙箱 / 虚拟机中处理。
- **模型与词表**：OpenVINO IR 与 `vocab.json` 由使用者提供。只使用可信来源
  （如 `OpenVINO/whisper-base-int8-ov`），并校验下载内容。
- **DLL 装载**：构建会把 OpenVINO 运行时复制到可执行文件目录。Windows 优先从程序
  目录加载 DLL，因此不要从不可信目录运行，也不要放在其他用户可写的目录。
- **输出覆盖**：`-o` 直接覆盖目标文件，脚本中请确认路径。

## 依赖与更新

- OpenVINO 运行时来自 `pip install openvino` 或 `OPENVINO_ROOT`，建议跟踪官方安全公告。
- 构建依赖（CMake、Ninja、MSYS2 UCRT64 GCC）按发行版渠道更新。
- 不在范围内：Whisper 模型本身的转写质量与错词，见 [README.md](README.md) 的「已知限制」。
