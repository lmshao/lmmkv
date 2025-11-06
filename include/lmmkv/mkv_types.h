/**
 * @author SHAO Liming <lmshao@163.com>
 * @copyright Copyright (c) 2025 SHAO Liming
 * @license MIT
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef LMSHAO_LMMKV_TYPES_H
#define LMSHAO_LMMKV_TYPES_H

#include <cstdint>
#include <map>
#include <string>
#include <utility>
#include <vector>

namespace lmshao::lmmkv {

// General MKV info parsed or to be written.
struct MkvInfo {
    uint64_t timecode_scale_ns = 1000000; // default 1ms
    double duration_seconds = 0.0;        // optional in streaming
};

// Track description for demux/mux.
struct MkvTrackInfo {
    uint64_t track_number = 0;
    std::string codec_id;   // e.g., "V_MPEG4/ISO/AVC", "V_MPEGH/ISO/HEVC", "A_AAC"
    std::string codec_name; // human readable
    std::map<std::string, std::string> metadata;

    // Video params
    uint32_t width = 0;
    uint32_t height = 0;

    // Audio params
    uint32_t sample_rate = 0;
    uint32_t channels = 0;

    // CodecPrivate raw bytes (avcC/hvcC/AAC ASC)
    std::vector<uint8_t> codec_private;
};

// Frame unit; for laced blocks, slices hold de-laced parts.
struct MkvFrame {
    uint64_t track_number = 0;
    int64_t timecode_ns = 0;
    bool keyframe = false;
    const uint8_t *data = nullptr;
    size_t size = 0;
    std::vector<std::pair<const uint8_t *, size_t>> slices;
};

} // namespace lmshao::lmmkv

#endif // LMSHAO_LMMKV_TYPES_H