/**
 * @author SHAO Liming <lmshao@163.com>
 * @copyright Copyright (c) 2025 SHAO Liming
 * @license MIT
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef LMSHAO_LMMKV_LISTENERS_H
#define LMSHAO_LMMKV_LISTENERS_H

#include "lmmkv/mkv_types.h"

namespace lmshao::lmmkv {

// Class-based demux listener to replace function callbacks.
class IMkvDemuxListener {
public:
    virtual ~IMkvDemuxListener() = default;

    // Called when Info parsed or updated
    virtual void OnInfo(const MkvInfo &info) = 0;

    // Called when a track is discovered
    virtual void OnTrack(const MkvTrackInfo &track) = 0;

    // Called for each decoded (de-laced) frame
    virtual void OnFrame(const MkvFrame &frame) = 0;

    // End of stream or segment
    virtual void OnEndOfStream() = 0;

    // Error with context
    virtual void OnError(int code, const std::string &msg) = 0;
};

// Class-based mux listener for observing output events.
class IMkvMuxListener {
public:
    virtual ~IMkvMuxListener() = default;

    virtual void OnSegmentStart() = 0;
    virtual void OnTrackWritten(const MkvTrackInfo &track) = 0;
    virtual void OnClusterStart(int64_t cluster_timecode_ns) = 0;
    virtual void OnClusterEnd(int64_t last_timecode_ns) = 0;
    virtual void OnError(int code, const std::string &msg) = 0;
};

} // namespace lmshao::lmmkv

#endif // LMSHAO_LMMKV_LISTENERS_H