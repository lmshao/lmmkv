/**
 * @author SHAO Liming <lmshao@163.com>
 * @copyright Copyright (c) 2025 SHAO Liming
 * @license MIT
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef LMSHAO_LMMKV_MKV_DEMUXER_H
#define LMSHAO_LMMKV_MKV_DEMUXER_H

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "lmcore/noncopyable.h"
#include "lmmkv/mkv_listeners.h"

namespace lmshao::lmmkv {

/**
 * @brief Matroska (MKV) Demuxer
 *
 * Extracts H.264 (Annex B) and AAC (ADTS) from MKV containers.
 * Public interface mirrors lmts::TSDemuxer style.
 */
class MkvDemuxer final : public lmcore::NonCopyable {
public:
    MkvDemuxer();
    ~MkvDemuxer();

    // Class-based listener
    void SetListener(const std::shared_ptr<IMkvDemuxListener> &listener);

    // Track filtering: only emit frames for selected tracks (empty = all)
    void SetTrackFilter(const std::vector<uint64_t> &tracks);

    // Lifecycle
    bool Start();
    void Stop();
    bool IsRunning() const;

    // Parse data buffer (streaming)
    size_t Consume(const uint8_t *data, size_t size);

    void Reset();

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace lmshao::lmmkv

#endif // LMSHAO_LMMKV_MKV_DEMUXER_H