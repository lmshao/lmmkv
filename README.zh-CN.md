# LMMKV — 轻量级 MKV 解析/分离库

LMMKV 是一个轻量级的 C++ 库，用于解析 Matroska (MKV) 容器并分离基础媒体流（H.264 Annex B 与 AAC ADTS）。库采用纯内存缓冲区的解析接口，依赖简单，易于集成。

## 特性

- 纯缓冲区解析：通过 `lmmkv::BufferCursor` 完成读取。
- 提取 H.264/AVC（Annex B）与 AAC/ADTS 帧。
- 在 `V_MPEGH/ISO/HEVC` 编码 ID 下支持 HEVC/H.265（Annex B）。
- 简单的监听器接口：`IMkvDemuxListener` 提供信息、轨道、帧与流结束回调。
- 支持轨道过滤，只输出指定轨道。
- MIT 许可证，源码简洁清晰。

## 构建

```bash
mkdir -p build
cd build
cmake ..
cmake --build . -j
```

## 示例

- `mkv_demuxer_demo`：分离帧并按轨道输出到文件。

```bash
./examples/mkv_demuxer_demo <input.mkv> [--tracks=N1,N2,...] [--outdir=DIR]
```

- `mkv_info`：打印基础信息（时间尺度、时长）与轨道信息。

```bash
./examples/mkv_info <input.mkv>
```

## 许可

MIT 许可。源文件头与 SPDX 标记已包含许可信息。