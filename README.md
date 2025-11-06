# LMMKV — Lightweight MKV Parsing/Demuxing Library

LMMKV is a lightweight C++ library for parsing Matroska (MKV) containers and demuxing elementary streams (H.264 Annex B and AAC ADTS). It focuses on a simple, buffer-only API and minimal dependencies.

## Features

- Buffer-only parsing via `lmmkv::BufferCursor` (no legacy stream reader).
- Extracts H.264/AVC (Annex B) and AAC/ADTS frames.
- Optional HEVC/H.265 support (Annex B) when codec ID is `V_MPEGH/ISO/HEVC`.
- Simple listener interface: `IMkvDemuxListener` for info, tracks, frames, and EOS.
- Track filtering to output only selected tracks.
- Clean MIT license.

## Build

```bash
mkdir -p build
cd build
cmake ..
cmake --build . -j
```

## Examples

- `mkv_demuxer_demo`: demuxes frames and writes per-track outputs.

```bash
./examples/mkv_demuxer_demo <input.mkv> [--tracks=N1,N2,...] [--outdir=DIR]
```

- `mkv_info`: prints basic info (timecode scale, duration) and tracks.

```bash
./examples/mkv_info <input.mkv>
```

## License

MIT. See the repository license headers and SPDX tags in sources.