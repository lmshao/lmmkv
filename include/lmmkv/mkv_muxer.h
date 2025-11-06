/**
 * @author SHAO Liming <lmshao@163.com>
 * @copyright Copyright (c) 2025 SHAO Liming
 * @license MIT
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef LMSHAO_LMMKV_MKV_MUXER_H
#define LMSHAO_LMMKV_MKV_MUXER_H

#include <cstdint>
#include <memory>
#include <vector>

#include "lmmkv/mkv_listeners.h"
#include "lmmkv/mkv_types.h"

namespace lmshao::lmmkv {

struct MkvMuxerOptions {
    uint64_t timecode_scale_ns = 1000000;
    bool write_seek_head = false;
    bool write_cues = false;
    uint32_t cluster_duration_ms = 1000;
    uint32_t cluster_size_bytes = 2 * 1024 * 1024;
    bool enable_lacing = false;
};

class MkvMuxer {
public:
    explicit MkvMuxer(const MkvMuxerOptions &opts);
    ~MkvMuxer();

    MkvMuxer(const MkvMuxer &) = delete;
    MkvMuxer &operator=(const MkvMuxer &) = delete;
    MkvMuxer(MkvMuxer &&) noexcept;
    MkvMuxer &operator=(MkvMuxer &&) noexcept;

    void SetListener(IMkvMuxListener *listener);
    // Writer interface will be provided by examples/utilities as needed
    void SetWriter(void *writer);

    bool AddTrack(const MkvTrackInfo &track);
    bool BeginSegment(const MkvInfo &info);
    bool WriteFrame(const MkvFrame &frame);
    bool EndSegment();
    void Reset();

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace lmshao::lmmkv

#endif // LMSHAO_LMMKV_MKV_MUXER_H