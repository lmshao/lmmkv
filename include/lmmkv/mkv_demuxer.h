/**
 * @author SHAO Liming <lmshao@163.com>
 * @copyright Copyright (c) 2025 SHAO Liming
 * @license MIT
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef LMSHAO_LMMKV_MKV_DEMUXER_H
#define LMSHAO_LMMKV_MKV_DEMUXER_H

#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "lmmkv/ebml_reader.h"
#include "lmmkv/mkv_listeners.h"
#include "lmmkv/mkv_types.h"

namespace lmshao::lmmkv {

/**
 * @brief Matroska (MKV) Demuxer
 *
 * Extracts H.264 (Annex B) and AAC (ADTS) from MKV containers.
 * Public interface mirrors lmts::TSDemuxer style.
 */
class MkvDemuxer {
public:
    MkvDemuxer();
    ~MkvDemuxer();

    // Non-copyable
    MkvDemuxer(const MkvDemuxer &) = delete;
    MkvDemuxer &operator=(const MkvDemuxer &) = delete;

    // Movable
    MkvDemuxer(MkvDemuxer &&) noexcept;
    MkvDemuxer &operator=(MkvDemuxer &&) noexcept;

    // Class-based listener
    void SetListener(IMkvDemuxListener *listener);

    // Track filtering: only emit frames for selected tracks (empty = all)
    void SetTrackFilter(const std::vector<uint64_t> &tracks);

    // Lifecycle
    bool Start();
    void Stop();
    bool IsRunning() const;

    // Parse data buffer (streaming). End-of-stream indicates no more data.
    size_t Consume(const uint8_t *data, size_t size, bool end_of_stream);

    // Stats
    std::unordered_map<std::string, uint64_t> GetStatistics() const;
    void ResetStatistics();
    void Reset();

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace lmshao::lmmkv

#endif // LMSHAO_LMMKV_MKV_DEMUXER_H