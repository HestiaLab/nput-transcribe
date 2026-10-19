# 第三方组件声明

本发布包（Windows x64 便携版）包含以下第三方二进制组件，取自 OpenVINO 运行时
发行包，未做修改：

| 组件 | 文件 | 许可证 |
| --- | --- | --- |
| OpenVINO 运行时 | `openvino.dll`、`openvino_c.dll`、`openvino_*_plugin.dll`、`openvino_*_frontend.dll` 等 | Apache License 2.0 |
| oneAPI Threading Building Blocks | `tbb12.dll`、`tbbbind_2_5.dll`、`tbbmalloc.dll`、`tbbmalloc_proxy.dll` | Apache License 2.0 |

许可全文见 `LICENSES/Apache-2.0.txt`。本项目源码使用 MIT 许可证，见 `LICENSE`。

`tests/vendor/unity` 仅用于源码仓库的单元测试，不包含在发布包中。

打包脚本会在本文件末尾附加实际使用的 OpenVINO 运行时版本。
