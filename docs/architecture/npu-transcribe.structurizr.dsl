workspace "npu-transcribe" "Whisper 语音转写 CLI：编码器在 Intel NPU，解码器在 CPU。当前状态架构模型。" {

    !identifiers hierarchical

    model {
        operator = person "命令行用户" "在本机选择音频、模型与输出格式，执行转写并读取文本。"
        integrator = person "库集成开发者" "通过 find_package(NpuTranscribe) 链接 nput_core，在自有程序中调用公共 ASR 接口。"

        nput = softwareSystem "npu-transcribe" "C11 控制台程序与可复用库：Whisper 编码器运行在 Intel NPU，解码器留在 CPU。" {
            cli = container "nput-transcribe CLI" "宽字符命令行转 UTF-8、参数解析、逐文件编排、txt/srt/json 输出。" "C11 / CMake" {
                cliEntry = component "CLI 入口与编排" "解析 argv、发现模型、准备语言与输出、逐文件逐片段调用 ASR。" "C11, src/main.c"
                cliArgs = component "参数解析辅助" "整数选项校验；完整选项表位于 main.c。" "C11, src/cli_args.c"
                cliOutput = component "结果格式化" "txt / srt / json 输出；JSON 字符串转义与 UTF-8 校验。" "C11, src/cli_output.c"
            }

            core = container "nput_core 库" "音频解码与重采样、log-mel、分段、ASR 会话与 Whisper 后端、BPE 解码、平台适配。" "C11 静态库（可选共享库）" {
                asrSession = component "ASR 会话层" "公共 ASR 接口、后端注册表、默认设备与 max_tokens 归一化、耗时统计。" "C11, src/nput/asr/asr.c"
                backendContract = component "ASR 后端接口（扩展点）" "四个函数指针：create / detect_language / transcribe / destroy。" "C11, src/nput/asr/backend.h"
                whisperBackend = component "Whisper 后端" "读取并编译 encoder/decoder IR、静态形状编码器、贪心解码循环、语言检测。" "C11, src/nput/asr/backend_whisper.c"
                ovDevices = component "OpenVINO 设备枚举" "--list-devices 查询本机可用设备。" "C11, src/nput/asr/ov_runtime.c"

                audioLoad = component "音频加载分发" ".wav/.wave 直接解析，其它扩展名交给平台解码器。" "C11, src/nput/audio/audio.c"
                wavDecoder = component "WAV 解析" "RIFF/WAVE，8/16/24/32 位 PCM 与 32/64 位浮点，混为单声道。" "C11, src/nput/audio/wav.c"
                mediaFoundation = component "Media Foundation 解码" "Windows 压缩音频与容器音轨解码为 16 位 PCM。" "C11, src/nput/platform/media_foundation.c"
                resampler = component "重采样" "窗函数 sinc 插值到 16 kHz 单声道。" "C11, src/nput/audio/resample.c"
                segmenter = component "能量分段" "上限前最后 2 秒内选最安静 20 ms 帧切分，片段不超过 30 秒。" "C11, src/nput/audio/segment.c"
                mel = component "log-mel 特征" "周期 Hann 窗、反射填充、80×3000、Whisper 归一化。" "C11, src/nput/audio/mel.c"

                tokenizer = component "字节级 BPE 解码" "加载 vocab.json，把 token id 还原为 UTF-8 文本。" "C11, src/nput/text/tokenizer.c"
                trigram = component "3-gram 重复抑制" "禁止已出现的 (prev2, prev1, candidate) 组合。" "header-only, src/nput/text/trigram.h"

                platform = component "平台文件与模型发现" "UTF-8 路径打开、父目录创建、./models 与 Hugging Face 缓存扫描。" "C11, src/nput/platform/platform.c"
                clock = component "单调时钟" "编码器与解码器耗时测量。" "C11, src/nput/platform/clock.c"
                status = component "状态码与错误详情" "公共状态码、线程局部错误文本、nput_free。" "C11, src/nput/core/status.c"
            }

            models = container "Whisper OpenVINO IR 目录" "openvino_encoder_model.xml/.bin、openvino_decoder_model.xml/.bin、vocab.json。" "文件" "Database"
            compileCache = container "OpenVINO 编译缓存" "NPU 首次编译产物；之后从缓存加载，约 20–30 秒降至 1–2 秒。" "文件" "Database"
            media = container "输入音频文件" "WAV，以及 Windows 上 MP3/M4A/FLAC/WMA 与带音轨的 MP4/MOV。" "文件" "Database"
        }

        openvino = softwareSystem "OpenVINO Runtime" "加载 IR、编译模型，并在 Intel NPU / CPU 上执行推理。" "External"
        mediaFoundationSystem = softwareSystem "Windows Media Foundation" "系统压缩音频解码器。" "External"
        modelSource = softwareSystem "Hugging Face 缓存 / 本地模型源" "为默认模型发现提供 Whisper IR 快照；由用户预先下载或安装。" "External"

        operator -> nput.cli "输入音频与参数，读取转写结果" "CLI"
        integrator -> nput.core "链接并调用公共 ASR / 音频 / 分词 API" "C API"
        operator -> modelSource "安装或下载模型" "文件"

        nput.cli -> nput.core "链接 nput_core，只包含 include/nput 公共头" "CMake / C ABI"
        nput.core -> nput.media "读取音频" "文件"
        nput.core -> nput.models "读取 IR 与词表" "文件"
        nput.core -> nput.compileCache "缓存编码器编译结果" "文件"
        nput.core -> openvino "编译并执行编码器/解码器" "OpenVINO C API"
        nput.core -> mediaFoundationSystem "调用系统解码器" "COM / Media Foundation"
        nput.core -> modelSource "扫描缓存中的模型快照" "文件系统"

        nput.cli.cliEntry -> nput.cli.cliArgs "解析整数选项"
        nput.cli.cliEntry -> nput.cli.cliOutput "写出 txt / srt / json"
        nput.cli.cliEntry -> nput.core.audioLoad "nput_audio_load"
        nput.cli.cliEntry -> nput.core.resampler "nput_audio_resample 到 16 kHz"
        nput.cli.cliEntry -> nput.core.segmenter "nput_segment_split，片段不超过 30 秒"
        nput.cli.cliEntry -> nput.core.asrSession "创建会话；逐片段检测语言与转写"
        nput.cli.cliEntry -> nput.core.platform "默认模型目录与输出父目录"
        nput.cli.cliEntry -> nput.core.whisperBackend "语言代码转换为 Whisper 语言 token"
        nput.cli.cliEntry -> nput.core.status "读取错误详情、释放返回值"

        nput.core.asrSession -> nput.core.backendContract "按名称选择并调用后端"
        nput.core.whisperBackend -> nput.core.backendContract "实现后端接口"
        nput.core.asrSession -> nput.core.whisperBackend "内置注册，当前唯一后端"
        nput.core.ovDevices -> openvino "ov_core_get_available_devices"

        nput.core.audioLoad -> nput.core.wavDecoder ".wav/.wave"
        nput.core.audioLoad -> nput.core.mediaFoundation "其它扩展名"
        nput.core.wavDecoder -> nput.core.platform "nput_file_open，UTF-8 转宽字符"
        nput.core.mediaFoundation -> mediaFoundationSystem "IMFSourceReader"
        nput.core.whisperBackend -> nput.core.mel "每片段计算 80×3000 log-mel"
        nput.core.whisperBackend -> nput.core.tokenizer "token id 转文本"
        nput.core.whisperBackend -> nput.core.trigram "贪心候选去重"
        nput.core.whisperBackend -> nput.core.clock "encoder_ms / decoder_ms"
        nput.core.whisperBackend -> nput.core.status "记录 OpenVINO 失败步骤"
        nput.core.whisperBackend -> openvino "编译 IR、设置张量、推理"
        nput.core.whisperBackend -> nput.models "读取 encoder / decoder IR 与 vocab.json"
        nput.core.whisperBackend -> nput.compileCache "CACHE_DIR"
        nput.core.platform -> nput.models "校验完整 IR 目录"
        nput.core.platform -> modelSource "扫描 snapshots"
    }

    views {
        systemContext nput "SystemContext" "系统上下文：使用者、外部运行时与模型来源。" {
            include *
            autoLayout lr
        }

        container nput "Containers" "容器：CLI、nput_core 与文件型数据存储。" {
            include *
            autoLayout lr
        }

        component nput.cli "CliComponents" "CLI 可执行文件内部组件。" {
            include *
            autoLayout tb
        }

        component nput.core "CoreComponents" "nput_core 内部组件与外部依赖。" {
            include *
            autoLayout tb
        }

        styles {
            element "Person" {
                shape person
                background #08427b
                color #ffffff
            }
            element "Database" {
                shape cylinder
                background #fff8e1
                color #4a3b12
            }
            element "External" {
                background #8a8a8a
                color #ffffff
            }
            element "Container" {
                background #438dd5
                color #ffffff
            }
            element "Component" {
                background #85bbf0
                color #102a43
            }
        }
    }
}
