/**
 * @author SHAO Liming <lmshao@163.com>
 * @copyright Copyright (c) 2025 SHAO Liming
 * @license MIT
 *
 * SPDX-License-Identifier: MIT
 */

#include "lmmkv/matroska_parser.h"

#include <cstring>

#include "ebml_reader.h"
#include "internal_logger.h"
#include "lmcore/byte_order.h"

namespace lmshao::lmmkv {

// Common EBML/Matroska element IDs (partial)
static constexpr uint64_t kEbmlHeaderId = 0x1A45DFA3ULL;  // EBML
static constexpr uint64_t kSegmentId = 0x18538067ULL;     // Segment
static constexpr uint64_t kInfoId = 0x1549A966ULL;        // Info
static constexpr uint64_t kTimecodeScaleId = 0x2AD7B1ULL; // TimecodeScale
static constexpr uint64_t kDurationId = 0x4489ULL;        // Duration

bool MatroskaParser::ParseBuffer(const uint8_t *data, size_t size, MatroskaInfo &info)
{
    BufferCursor cur(data, size);
    EbmlElementHeader hdr{};
    if (!NextElement(cur, hdr)) {
        LMMKV_LOGE("Failed to read first EBML element header");
        return false;
    }
    if (hdr.id != kEbmlHeaderId) {
        LMMKV_LOGE("Unexpected first element ID: 0x%llX, expected EBML", (unsigned long long)hdr.id);
        return false;
    }
    // Skip EBML header payload
    size_t pos = cur.Tell();
    if (!cur.Seek(pos + static_cast<size_t>(hdr.size))) {
        LMMKV_LOGE("Failed to skip EBML header payload size=%llu", (unsigned long long)hdr.size);
        return false;
    }

    // Read next element: should be Segment
    if (!NextElement(cur, hdr)) {
        LMMKV_LOGE("Failed to read Segment header");
        return false;
    }
    if (hdr.id != kSegmentId) {
        LMMKV_LOGE("Unexpected second element ID: 0x%llX, expected Segment", (unsigned long long)hdr.id);
        return false;
    }

    // Inside Segment, scan for Info element (minimal)
    size_t segment_start = cur.Tell();
    size_t segment_end = segment_start + static_cast<size_t>(hdr.size);
    while (cur.Tell() < segment_end) {
        EbmlElementHeader child{};
        if (!NextElement(cur, child)) {
            LMMKV_LOGW("End of segment or failed to read child header");
            break;
        }
        if (child.id == kInfoId) {
            // Iterate fields within Info in a minimal way
            size_t info_start = cur.Tell();
            size_t info_end = info_start + static_cast<size_t>(child.size);

            // Use lmcore::ByteOrder to convert big-endian IEEE-754 floats
            using lmshao::lmcore::ByteOrder;
            auto ReadBEFloat32 = [](const uint8_t *p) -> float {
                uint32_t bits = ByteOrder::ReadBE32(p);
                float f;
                std::memcpy(&f, &bits, sizeof(f));
                return f;
            };
            auto ReadBEFloat64 = [](const uint8_t *p) -> double {
                uint64_t bits = ByteOrder::ReadBE64(p);
                double d;
                std::memcpy(&d, &bits, sizeof(d));
                return d;
            };

            while (cur.Tell() < info_end) {
                EbmlElementHeader kv{};
                if (!NextElement(cur, kv)) {
                    break;
                }
                if (kv.id == kTimecodeScaleId) {
                    // TimecodeScale is an integer (default 1_000_000)
                    uint8_t buf[8] = {0};
                    size_t to_read = static_cast<size_t>(kv.size);
                    if (to_read > sizeof(buf))
                        to_read = sizeof(buf);
                    size_t r = cur.Read(buf, to_read);
                    if (r == to_read && to_read > 0) {
                        uint64_t v = 0;
                        for (size_t i = 0; i < to_read; ++i) {
                            v = (v << 8) | buf[i];
                        }
                        info.timecode_scale_ns = v;
                    }
                } else if (kv.id == kDurationId) {
                    // Duration is a float (size=4 or 8)
                    uint8_t buf[8] = {0};
                    size_t to_read = static_cast<size_t>(kv.size);
                    if (to_read > sizeof(buf))
                        to_read = sizeof(buf);
                    size_t r = cur.Read(buf, to_read);
                    if (r == to_read && (to_read == 4 || to_read == 8)) {
                        if (to_read == 4) {
                            info.duration_seconds = static_cast<double>(ReadBEFloat32(buf));
                        } else {
                            info.duration_seconds = ReadBEFloat64(buf);
                        }
                    }
                } else {
                    // Skip unknown field
                    size_t pos = cur.Tell();
                    cur.Seek(pos + static_cast<size_t>(kv.size));
                }
            }
            // Move to end of Info
            cur.Seek(info_end);
        } else {
            // Skip element we do not parse yet
            size_t pos = cur.Tell();
            cur.Seek(pos + static_cast<size_t>(child.size));
        }
    }

    LMMKV_LOGI("Parsed Matroska: timecode_scale=%llu ns, duration=%.3f s", (unsigned long long)info.timecode_scale_ns,
               info.duration_seconds);
    return true;
}

} // namespace lmshao::lmmkv