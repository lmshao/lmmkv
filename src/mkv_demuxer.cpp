/**
 * @author SHAO Liming <lmshao@163.com>
 * @copyright Copyright (c) 2025 SHAO Liming
 * @license MIT
 *
 * SPDX-License-Identifier: MIT
 */

#include "lmmkv/mkv_demuxer.h"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstring>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "internal_logger.h"
#include "lmmkv/ebml_reader.h"
#include "lmmkv/mkv_listeners.h"
#include "lmmkv/mkv_types.h"

namespace lmshao::lmmkv {

// Matroska/EBML element IDs
static constexpr uint64_t kSegmentId = 0x18538067ULL;       // Segment
static constexpr uint64_t kInfoId = 0x1549A966ULL;          // Info
static constexpr uint64_t kTracksId = 0x1654AE6BULL;        // Tracks
static constexpr uint64_t kTrackEntryId = 0xAEULL;          // TrackEntry
static constexpr uint64_t kTrackNumberId = 0xD7ULL;         // TrackNumber
static constexpr uint64_t kTrackTypeId = 0x83ULL;           // TrackType
static constexpr uint64_t kCodecId = 0x86ULL;               // CodecID
static constexpr uint64_t kCodecPrivate = 0x63A2ULL;        // CodecPrivate
static constexpr uint64_t kAudioId = 0xE1ULL;               // Audio
static constexpr uint64_t kChannelsId = 0x9FULL;            // Channels
static constexpr uint64_t kSamplingFreqId = 0xB5ULL;        // SamplingFrequency
static constexpr uint64_t kClusterId = 0x1F43B675ULL;       // Cluster
static constexpr uint64_t kClusterTimecodeId = 0xE7ULL;     // Timecode
static constexpr uint64_t kSimpleBlockId = 0xA3ULL;         // SimpleBlock
static constexpr uint64_t kBlockGroupId = 0xA0ULL;          // BlockGroup
static constexpr uint64_t kBlockId = 0xA1ULL;               // Block
static constexpr uint64_t kDefaultDurationId = 0x23E383ULL; // DefaultDuration
static constexpr uint64_t kVideoId = 0xE0ULL;               // Video
static constexpr uint64_t kPixelWidthId = 0xB0ULL;          // PixelWidth
static constexpr uint64_t kPixelHeightId = 0xBAULL;         // PixelHeight

// Track types
static constexpr uint8_t kTrackTypeVideo = 0x01;
static constexpr uint8_t kTrackTypeAudio = 0x02;

// Helpers (buffer-only)
static inline size_t ReadBytes(BufferCursor &cur, uint8_t *dst, size_t n)
{
    size_t r = cur.Read(dst, n);
    if (r != n) {
        LMMKV_LOGE("Failed to read %zu bytes, got %zu", n, r);
    }
    return r;
}

static inline bool SkipBytes(BufferCursor &cur, size_t n)
{
    size_t pos = cur.Tell();
    return cur.Seek(pos + n);
}

static inline uint64_t ReadUnsignedBE(BufferCursor &cur, size_t size)
{
    uint64_t v = 0;
    for (size_t i = 0; i < size; ++i) {
        uint8_t b = 0;
        if (ReadBytes(cur, &b, 1) != 1) {
            return 0;
        }
        v = (v << 8) | b;
    }
    return v;
}

static inline double ReadFloatBE(BufferCursor &cur, size_t size)
{
    if (size == 4) {
        uint32_t be = static_cast<uint32_t>(ReadUnsignedBE(cur, 4));
        uint32_t le = ((be & 0x000000FFu) << 24) | ((be & 0x0000FF00u) << 8) | ((be & 0x00FF0000u) >> 8) |
                      ((be & 0xFF000000u) >> 24);
        float f;
        std::memcpy(&f, &le, sizeof(f));
        return static_cast<double>(f);
    } else if (size == 8) {
        uint64_t be = ReadUnsignedBE(cur, 8);
        uint64_t le = ((be & 0x00000000000000FFULL) << 56) | ((be & 0x000000000000FF00ULL) << 40) |
                      ((be & 0x0000000000FF0000ULL) << 24) | ((be & 0x00000000FF000000ULL) << 8) |
                      ((be & 0x000000FF00000000ULL) >> 8) | ((be & 0x0000FF0000000000ULL) >> 24) |
                      ((be & 0x00FF000000000000ULL) >> 40) | ((be & 0xFF00000000000000ULL) >> 56);
        double d;
        std::memcpy(&d, &le, sizeof(d));
        return d;
    }
    // Unsupported size
    SkipBytes(cur, size);
    return 0.0;
}

static inline std::vector<uint8_t> ReadPayload(BufferCursor &cur, size_t size)
{
    std::vector<uint8_t> buf;
    buf.resize(size);
    if (size > 0) {
        size_t r = cur.Read(buf.data(), size);
        if (r != size) {
            LMMKV_LOGE("Failed to read payload size=%zu (got %zu)", size, r);
            buf.resize(r);
        }
    }
    return buf;
}

// Track info
struct TrackInfo {
    uint64_t track_number;
    uint8_t track_type; // 1 video, 2 audio
    std::string codec_id;
    std::vector<uint8_t> codec_private;

    // H264 avcC
    uint8_t nal_length_size; // 1/2/4
    std::vector<std::vector<uint8_t>> sps_list;
    std::vector<std::vector<uint8_t>> pps_list;

    // HEVC hvcC
    std::vector<std::vector<uint8_t>> vps_list;
    std::vector<std::vector<uint8_t>> sps_hevc_list;
    std::vector<std::vector<uint8_t>> pps_hevc_list;
    uint8_t nal_length_size_hevc{4};

    // AAC ASC
    uint8_t aac_object_type; // raw
    uint8_t aac_profile;     // profile-1
    uint8_t aac_sample_rate_index;
    uint32_t aac_sample_rate;
    uint8_t aac_channel_config;

    // Matroska metadata
    uint64_t default_duration_ns{0};
    uint32_t pixel_width{0};
    uint32_t pixel_height{0};

    TrackInfo()
        : track_number(0), track_type(0), nal_length_size(4), aac_object_type(2), aac_profile(1),
          aac_sample_rate_index(4), aac_sample_rate(44100), aac_channel_config(2)
    {
    }
};

static inline bool StartsWith(const std::string &s, const char *prefix)
{
    return s.size() >= std::strlen(prefix) && std::equal(prefix, prefix + std::strlen(prefix), s.begin());
}

static inline void ParseAvcC(TrackInfo &ti)
{
    const auto &cp = ti.codec_private;
    if (cp.size() < 7) {
        LMMKV_LOGW("avcC too short: %zu", cp.size());
        return;
    }
    uint8_t configurationVersion = cp[0];
    (void)configurationVersion;
    uint8_t lengthSizeMinusOne = cp[4] & 0x03;
    ti.nal_length_size = static_cast<uint8_t>(lengthSizeMinusOne + 1);
    uint8_t numSps = cp[5] & 0x1F;
    size_t offset = 6;
    for (uint8_t i = 0; i < numSps; ++i) {
        if (offset + 2 > cp.size())
            return;
        uint16_t spsLen = static_cast<uint16_t>((cp[offset] << 8) | cp[offset + 1]);
        offset += 2;
        if (offset + spsLen > cp.size())
            return;
        ti.sps_list.emplace_back(cp.begin() + offset, cp.begin() + offset + spsLen);
        offset += spsLen;
    }
    if (offset + 1 > cp.size())
        return;
    uint8_t numPps = cp[offset];
    offset += 1;
    for (uint8_t i = 0; i < numPps; ++i) {
        if (offset + 2 > cp.size())
            return;
        uint16_t ppsLen = static_cast<uint16_t>((cp[offset] << 8) | cp[offset + 1]);
        offset += 2;
        if (offset + ppsLen > cp.size())
            return;
        ti.pps_list.emplace_back(cp.begin() + offset, cp.begin() + offset + ppsLen);
        offset += ppsLen;
    }
}

// HEVC hvcC parsing (minimal)
static inline void ParseHvcC(TrackInfo &ti)
{
    const auto &cp = ti.codec_private;
    if (cp.size() < 23) {
        LMMKV_LOGW("hvcC too short: %zu", cp.size());
        return;
    }
    // lengthSizeMinusOne is at byte 21 in ISO/IEC 14496-15 (HEVC)
    ti.nal_length_size_hevc = static_cast<uint8_t>((cp[21] & 0x03) + 1);
    size_t offset = 22;
    uint8_t numArrays = cp[offset++];
    for (uint8_t ai = 0; ai < numArrays; ++ai) {
        if (offset + 3 > cp.size())
            return;
        uint8_t arrayCompleteness = (cp[offset] & 0x80) >> 7;
        uint8_t nalUnitType = cp[offset] & 0x3F; // 32..34..
        (void)arrayCompleteness;
        offset += 1;
        uint16_t numNalus = static_cast<uint16_t>((cp[offset] << 8) | cp[offset + 1]);
        offset += 2;
        for (uint16_t ni = 0; ni < numNalus; ++ni) {
            if (offset + 2 > cp.size())
                return;
            uint16_t nalSize = static_cast<uint16_t>((cp[offset] << 8) | cp[offset + 1]);
            offset += 2;
            if (offset + nalSize > cp.size())
                return;
            std::vector<uint8_t> nal(cp.begin() + offset, cp.begin() + offset + nalSize);
            offset += nalSize;
            if (nalUnitType == 32) {
                ti.vps_list.emplace_back(std::move(nal));
            } else if (nalUnitType == 33) {
                ti.sps_hevc_list.emplace_back(std::move(nal));
            } else if (nalUnitType == 34) {
                ti.pps_hevc_list.emplace_back(std::move(nal));
            }
        }
    }
}

static inline void AppendStartCode(std::vector<uint8_t> &out);

static inline std::vector<uint8_t> ConvertHvccFrameToAnnexB(const TrackInfo &ti, const uint8_t *data, size_t size,
                                                            bool keyframe)
{
    std::vector<uint8_t> out;
    if (keyframe) {
        for (const auto &vps : ti.vps_list) {
            AppendStartCode(out);
            out.insert(out.end(), vps.begin(), vps.end());
        }
        for (const auto &sps : ti.sps_hevc_list) {
            AppendStartCode(out);
            out.insert(out.end(), sps.begin(), sps.end());
        }
        for (const auto &pps : ti.pps_hevc_list) {
            AppendStartCode(out);
            out.insert(out.end(), pps.begin(), pps.end());
        }
    }
    size_t offset = 0;
    while (offset + ti.nal_length_size_hevc <= size) {
        uint32_t nalLen = 0;
        if (ti.nal_length_size_hevc == 1) {
            nalLen = data[offset];
        } else if (ti.nal_length_size_hevc == 2) {
            nalLen = static_cast<uint32_t>((data[offset] << 8) | data[offset + 1]);
        } else if (ti.nal_length_size_hevc == 4) {
            nalLen = (static_cast<uint32_t>(data[offset]) << 24) | (static_cast<uint32_t>(data[offset + 1]) << 16) |
                     (static_cast<uint32_t>(data[offset + 2]) << 8) | static_cast<uint32_t>(data[offset + 3]);
        } else {
            break;
        }
        offset += ti.nal_length_size_hevc;
        if (offset + nalLen > size)
            break;
        AppendStartCode(out);
        out.insert(out.end(), data + offset, data + offset + nalLen);
        offset += nalLen;
    }
    return out;
}
// AAC AudioSpecificConfig parsing (basic)
static inline void ParseAacAsc(TrackInfo &ti)
{
    const auto &cp = ti.codec_private;
    if (cp.empty())
        return;
    // Read first 2 bytes
    uint16_t u = 0;
    if (cp.size() >= 2) {
        u = static_cast<uint16_t>((cp[0] << 8) | cp[1]);
    } else {
        u = static_cast<uint16_t>(cp[0] << 8);
    }
    uint8_t audioObjectType = static_cast<uint8_t>((cp[0] >> 3) & 0x1F);
    uint8_t samplingFrequencyIndex = static_cast<uint8_t>(((cp[0] & 0x07) << 1) | ((cp[1] >> 7) & 0x01));
    uint8_t channelConfig = static_cast<uint8_t>((cp[1] >> 3) & 0x0F);

    ti.aac_object_type = audioObjectType;
    ti.aac_profile = static_cast<uint8_t>(audioObjectType - 1);
    ti.aac_sample_rate_index = samplingFrequencyIndex;
    ti.aac_channel_config = channelConfig;

    static const uint32_t kSampleRates[16] = {96000, 88200, 64000, 48000, 44100, 32000, 24000, 22050,
                                              16000, 12000, 11025, 8000,  7350,  0,     0,     0};
    ti.aac_sample_rate = (samplingFrequencyIndex < 16) ? kSampleRates[samplingFrequencyIndex] : 0;
}

static inline void AppendStartCode(std::vector<uint8_t> &out)
{
    static const uint8_t kStartCode[4] = {0x00, 0x00, 0x00, 0x01};
    out.insert(out.end(), kStartCode, kStartCode + 4);
}

static inline std::vector<uint8_t> ConvertAvccFrameToAnnexB(const TrackInfo &ti, const uint8_t *data, size_t size,
                                                            bool keyframe)
{
    std::vector<uint8_t> out;
    if (keyframe) {
        for (const auto &sps : ti.sps_list) {
            AppendStartCode(out);
            out.insert(out.end(), sps.begin(), sps.end());
        }
        for (const auto &pps : ti.pps_list) {
            AppendStartCode(out);
            out.insert(out.end(), pps.begin(), pps.end());
        }
    }
    size_t offset = 0;
    while (offset + ti.nal_length_size <= size) {
        uint32_t nalLen = 0;
        if (ti.nal_length_size == 1) {
            nalLen = data[offset];
        } else if (ti.nal_length_size == 2) {
            nalLen = static_cast<uint32_t>((data[offset] << 8) | data[offset + 1]);
        } else if (ti.nal_length_size == 4) {
            nalLen = (static_cast<uint32_t>(data[offset]) << 24) | (static_cast<uint32_t>(data[offset + 1]) << 16) |
                     (static_cast<uint32_t>(data[offset + 2]) << 8) | static_cast<uint32_t>(data[offset + 3]);
        } else {
            break;
        }
        offset += ti.nal_length_size;
        if (offset + nalLen > size) {
            break;
        }
        AppendStartCode(out);
        out.insert(out.end(), data + offset, data + offset + nalLen);
        offset += nalLen;
    }
    return out;
}

static inline std::vector<uint8_t> BuildAdtsHeader(const TrackInfo &ti, size_t aac_payload_size)
{
    std::vector<uint8_t> hdr(7);
    uint16_t frameLen = static_cast<uint16_t>(aac_payload_size + 7);
    // Byte 0-1: sync + flags
    hdr[0] = 0xFF;
    hdr[1] = 0xF1; // 1111 0001: MPEG-4, no CRC
    // Byte 2: profile(2) + sampling_frequency_index(4) + private_bit(1) + channel_config high(1)
    hdr[2] = static_cast<uint8_t>(((ti.aac_profile & 0x03) << 6) | ((ti.aac_sample_rate_index & 0x0F) << 2) |
                                  ((ti.aac_channel_config >> 2) & 0x01));
    // Byte 3: channel_config low(2) + original/copy(1) + home(1) + copyright bits(2) + frame length high(2)
    hdr[3] = static_cast<uint8_t>(((ti.aac_channel_config & 0x03) << 6) | ((frameLen >> 11) & 0x03));
    // Byte 4: frame length mid 8 bits
    hdr[4] = static_cast<uint8_t>((frameLen >> 3) & 0xFF);
    // Byte 5: frame length low 3 bits + fullness high 5 bits
    hdr[5] = static_cast<uint8_t>(((frameLen & 0x07) << 5) | 0x1F);
    // Byte 6: fullness low 8 bits + num_raw_blocks(2)
    hdr[6] = static_cast<uint8_t>(0xFC); // 0x7FF fullness (VBR), num_blocks=0
    return hdr;
}

class MkvDemuxer::Impl {
public:
    Impl() : running_(false), timecode_scale_ns_(1000000), current_cluster_timecode_ns_(0) {}
    ~Impl() { Stop(false); }
    void SetTrackFilter(const std::vector<uint64_t> &tracks)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        track_filter_.clear();
        for (auto t : tracks) {
            track_filter_.insert(t);
        }
    }

    bool Start()
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (running_) {
            LMMKV_LOGW("Demuxer already running");
            return true;
        }
        running_ = true;
        Reset();
        LMMKV_LOGI("MKV Demuxer started");
        return true;
    }

    void Stop() { Stop(true); }

    void Stop(bool notify)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!running_)
            return;
        running_ = false;
        LMMKV_LOGI("MKV Demuxer stopped");
        if (notify && listener_)
            listener_->OnEndOfStream();
    }

    bool IsRunning() const
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return running_;
    }

    size_t ParseData(const uint8_t *data, size_t size)
    {
        BufferCursor cur(data, size);
        DemuxCursor(cur);
        statistics_["bytes_processed"] += size;
        return size;
    }

    bool DemuxCursor(BufferCursor &cur)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!running_) {
            LMMKV_LOGE("Demuxer not running");
            return false;
        }

        EbmlElementHeader hdr{};
        // Find Segment
        if (!NextElement(cur, hdr)) {
            LMMKV_LOGE("Failed to read first element");
            return false;
        }
        if (hdr.id != kSegmentId) {
            // There may be EBML header first; skip until Segment
            size_t after_first = cur.Tell();
            if (!cur.Seek(after_first + static_cast<size_t>(hdr.size))) {
                LMMKV_LOGE("Failed to skip first element");
                return false;
            }
            bool found = false;
            while (NextElement(cur, hdr)) {
                if (hdr.id == kSegmentId) {
                    found = true;
                    break;
                }
                size_t payload_end = cur.Tell() + static_cast<size_t>(hdr.size);
                if (!cur.Seek(payload_end)) {
                    break;
                }
            }
            if (!found) {
                LMMKV_LOGE("Segment not found");
                return false;
            }
        }

        size_t seg_end = cur.Tell() + static_cast<size_t>(hdr.size);
        while (cur.Tell() < seg_end) {
            size_t before = cur.Tell();
            if (!NextElement(cur, hdr))
                break;
            size_t payload_end = cur.Tell() + static_cast<size_t>(hdr.size);
            if (hdr.id == kInfoId) {
                ParseInfo(cur, hdr.size);
            } else if (hdr.id == kTracksId) {
                ParseTracks(cur, hdr.size);
            } else if (hdr.id == kClusterId) {
                ParseCluster(cur, hdr.size);
            } else {
                statistics_["elements_skipped"]++;
            }
            if (!cur.Seek(payload_end))
                break;
        }

        return true;
    }

    // Removed IByteReader adapter; library consumes Input directly

    void SetListener(IMkvDemuxListener *l) { listener_ = l; }

    std::unordered_map<std::string, uint64_t> GetStatistics() const { return statistics_; }
    void ResetStatistics() { statistics_.clear(); }
    void Reset()
    {
        tracks_.clear();
        statistics_.clear();
        timecode_scale_ns_ = 1000000;
        current_cluster_timecode_ns_ = 0;
    }

private:
    void ParseInfo(BufferCursor &cur, uint64_t size)
    {
        size_t end = cur.Tell() + static_cast<size_t>(size);
        EbmlElementHeader sub{};
        while (cur.Tell() < end) {
            if (!NextElement(cur, sub))
                break;
            if (sub.id == 0x2AD7B1ULL) { // TimecodeScale
                // unsigned integer
                timecode_scale_ns_ = ReadUnsignedBE(cur, static_cast<size_t>(sub.size));
            } else if (sub.id == 0x4489ULL) { // Duration (float)
                (void)ReadFloatBE(cur, static_cast<size_t>(sub.size));
            } else {
                SkipBytes(cur, static_cast<size_t>(sub.size));
            }
        }
        LMMKV_LOGI("Info: TimecodeScale=%llu ns", (unsigned long long)timecode_scale_ns_);
        if (listener_) {
            MkvInfo info;
            info.timecode_scale_ns = timecode_scale_ns_;
            info.duration_seconds = 0.0; // not computed in streaming
            listener_->OnInfo(info);
        }
    }

    void ParseTracks(BufferCursor &cur, uint64_t size)
    {
        size_t end = cur.Tell() + static_cast<size_t>(size);
        EbmlElementHeader sub{};
        while (cur.Tell() < end) {
            if (!NextElement(cur, sub))
                break;
            size_t payload_end = cur.Tell() + static_cast<size_t>(sub.size);
            if (sub.id == kTrackEntryId) {
                ParseTrackEntry(cur, sub.size);
            } else {
                // skip unknown track-level elements
            }
            if (!cur.Seek(payload_end))
                break;
        }
    }

    void ParseTrackEntry(BufferCursor &cur, uint64_t size)
    {
        size_t end = cur.Tell() + static_cast<size_t>(size);
        EbmlElementHeader sub{};
        TrackInfo ti;
        while (cur.Tell() < end) {
            if (!NextElement(cur, sub))
                break;
            if (sub.id == kTrackNumberId) {
                ti.track_number = ReadUnsignedBE(cur, static_cast<size_t>(sub.size));
            } else if (sub.id == kTrackTypeId) {
                ti.track_type = static_cast<uint8_t>(ReadUnsignedBE(cur, static_cast<size_t>(sub.size)) & 0xFF);
            } else if (sub.id == kCodecId) {
                auto payload = ReadPayload(cur, static_cast<size_t>(sub.size));
                ti.codec_id.assign(payload.begin(), payload.end());
                // trim trailing nulls
                while (!ti.codec_id.empty() && ti.codec_id.back() == '\0')
                    ti.codec_id.pop_back();
            } else if (sub.id == kCodecPrivate) {
                ti.codec_private = ReadPayload(cur, static_cast<size_t>(sub.size));
            } else if (sub.id == kDefaultDurationId) {
                ti.default_duration_ns = ReadUnsignedBE(cur, static_cast<size_t>(sub.size));
            } else if (sub.id == kAudioId) {
                // parse nested audio for sample rate/channels
                size_t a_end = cur.Tell() + static_cast<size_t>(sub.size);
                EbmlElementHeader a_sub{};
                while (cur.Tell() < a_end) {
                    if (!NextElement(cur, a_sub))
                        break;
                    if (a_sub.id == kChannelsId) {
                        ti.aac_channel_config =
                            static_cast<uint8_t>(ReadUnsignedBE(cur, static_cast<size_t>(a_sub.size)) & 0xFF);
                    } else if (a_sub.id == kSamplingFreqId) {
                        double sf = ReadFloatBE(cur, static_cast<size_t>(a_sub.size));
                        ti.aac_sample_rate = static_cast<uint32_t>(sf + 0.5);
                        // map to index roughly
                        static const uint32_t rates[16] = {96000, 88200, 64000, 48000, 44100, 32000, 24000, 22050,
                                                           16000, 12000, 11025, 8000,  7350,  0,     0,     0};
                        for (uint8_t i = 0; i < 16; ++i)
                            if (rates[i] == ti.aac_sample_rate)
                                ti.aac_sample_rate_index = i;
                    } else {
                        SkipBytes(cur, static_cast<size_t>(a_sub.size));
                    }
                }
            } else if (sub.id == kVideoId) {
                // parse nested video for dimensions
                size_t v_end = cur.Tell() + static_cast<size_t>(sub.size);
                EbmlElementHeader v_sub{};
                while (cur.Tell() < v_end) {
                    if (!NextElement(cur, v_sub))
                        break;
                    if (v_sub.id == kPixelWidthId) {
                        ti.pixel_width = static_cast<uint32_t>(ReadUnsignedBE(cur, static_cast<size_t>(v_sub.size)));
                    } else if (v_sub.id == kPixelHeightId) {
                        ti.pixel_height = static_cast<uint32_t>(ReadUnsignedBE(cur, static_cast<size_t>(v_sub.size)));
                    } else {
                        SkipBytes(cur, static_cast<size_t>(v_sub.size));
                    }
                }
            } else {
                SkipBytes(cur, static_cast<size_t>(sub.size));
            }
        }

        // If H264: parse avcC
        if (StartsWith(ti.codec_id, "V_MPEG4/ISO/AVC")) {
            ParseAvcC(ti);
        }
        // If HEVC: parse hvcC
        if (StartsWith(ti.codec_id, "V_MPEGH/ISO/HEVC") || StartsWith(ti.codec_id, "V_MPEGH/ISO/HEVC")) {
            ParseHvcC(ti);
        }
        // If AAC: parse ASC
        if (StartsWith(ti.codec_id, "A_AAC")) {
            ParseAacAsc(ti);
        }

        tracks_[ti.track_number] = ti;

        // Legacy StreamInfo callback removed.

        // Emit class-based track info
        if (listener_) {
            MkvTrackInfo t;
            t.track_number = ti.track_number;
            t.codec_id = ti.codec_id;
            t.codec_name = ti.codec_id;
            if (ti.track_type == kTrackTypeVideo) {
                t.metadata["type"] = "video";
                t.width = ti.pixel_width;
                t.height = ti.pixel_height;
            }
            if (ti.track_type == kTrackTypeAudio) {
                t.metadata["type"] = "audio";
                t.sample_rate = ti.aac_sample_rate;
                t.channels = ti.aac_channel_config;
            }
            t.metadata["timecode_scale_ns"] = std::to_string(timecode_scale_ns_);
            t.codec_private = ti.codec_private;
            listener_->OnTrack(t);
        }
    }

    void ParseCluster(BufferCursor &cur, uint64_t size)
    {
        size_t end = cur.Tell() + static_cast<size_t>(size);
        EbmlElementHeader sub{};
        while (cur.Tell() < end) {
            if (!NextElement(cur, sub))
                break;
            if (sub.id == kClusterTimecodeId) {
                uint64_t tc = ReadUnsignedBE(cur, static_cast<size_t>(sub.size));
                current_cluster_timecode_ns_ = tc * timecode_scale_ns_;
            } else if (sub.id == kSimpleBlockId) {
                ParseSimpleBlock(cur, sub.size);
            } else if (sub.id == kBlockGroupId) {
                // Minimal: skip BlockGroup for now
                SkipBytes(cur, static_cast<size_t>(sub.size));
            } else {
                SkipBytes(cur, static_cast<size_t>(sub.size));
            }
        }
    }

    void ParseSimpleBlock(BufferCursor &cur, uint64_t size)
    {
        size_t block_end = cur.Tell() + static_cast<size_t>(size);
        // TrackNumber (vint, strip leading 1-bits like size)
        uint64_t track_number = 0;
        size_t tn_len = ReadVintSize(cur, track_number);
        if (tn_len == 0) {
            SkipBytes(cur, static_cast<size_t>(size));
            return;
        }
        // Timecode (signed 16-bit, big endian)
        uint8_t tcb[2] = {0};
        if (ReadBytes(cur, tcb, 2) != 2)
            return;
        int16_t rel_tc = static_cast<int16_t>((tcb[0] << 8) | tcb[1]);
        // Flags
        uint8_t flags = 0;
        if (ReadBytes(cur, &flags, 1) != 1)
            return;
        bool keyframe = (flags & 0x80) != 0;
        uint8_t lacing = (flags & 0x06) >> 1; // 0=no lacing, 1=xiph,2=fixed,3=ebml
        std::vector<std::vector<uint8_t>> payloads;
        std::vector<uint64_t> payload_timestamps;
        if (lacing == 0) {
            size_t payload_size = block_end - cur.Tell();
            payloads.emplace_back(ReadPayload(cur, payload_size));
            payload_timestamps.push_back(0);
        } else if (lacing == 1) {
            // Xiph lacing: num_frames = 1 + next byte, sizes encoded as series of bytes summing to size
            uint8_t num_frames_minus1 = 0;
            if (ReadBytes(cur, &num_frames_minus1, 1) != 1)
                return;
            uint8_t num_frames = static_cast<uint8_t>(num_frames_minus1 + 1);
            std::vector<size_t> sizes;
            sizes.reserve(num_frames);
            for (uint8_t fi = 0; fi < num_frames - 1; ++fi) {
                size_t sz = 0;
                while (true) {
                    uint8_t b = 0;
                    if (ReadBytes(cur, &b, 1) != 1)
                        return;
                    sz += b;
                    if (b != 255)
                        break;
                }
                sizes.push_back(sz);
            }
            // last frame size is remainder
            size_t consumed = 0;
            for (size_t i = 0; i < sizes.size(); ++i)
                consumed += sizes[i];
            size_t remaining = block_end - cur.Tell();
            size_t last_sz = remaining - consumed;
            sizes.push_back(last_sz);
            for (size_t sz : sizes) {
                payloads.emplace_back(ReadPayload(cur, sz));
                payload_timestamps.push_back(0);
            }
        } else if (lacing == 2) {
            // Fixed-size lacing: num_frames = 1 + next byte, then equally sized frames
            uint8_t num_frames_minus1 = 0;
            if (ReadBytes(cur, &num_frames_minus1, 1) != 1)
                return;
            uint8_t num_frames = static_cast<uint8_t>(num_frames_minus1 + 1);
            size_t total = block_end - cur.Tell();
            size_t per = total / num_frames;
            for (uint8_t fi = 0; fi < num_frames; ++fi) {
                payloads.emplace_back(ReadPayload(cur, per));
                payload_timestamps.push_back(0);
            }
        } else if (lacing == 3) {
            // EBML lacing: num_frames = 1 + next byte, first size as EBML vint, then deltas as signed
            uint8_t num_frames_minus1 = 0;
            if (ReadBytes(cur, &num_frames_minus1, 1) != 1)
                return;
            uint8_t num_frames = static_cast<uint8_t>(num_frames_minus1 + 1);
            uint64_t first_size = 0;
            size_t vsz = ReadVintSize(cur, first_size);
            if (vsz == 0)
                return;
            auto read_signed_vint = [&](int64_t &out) -> bool {
                uint64_t u = 0;
                size_t w = ReadVintSize(cur, u);
                if (w == 0)
                    return false;
                // Mapping described in Matroska notes: signed = unsigned - (2^((7*w)-1) - 1)
                uint64_t bias = (w >= 1) ? ((1ULL << (7 * w - 1)) - 1ULL) : 0ULL;
                out = static_cast<int64_t>(static_cast<int64_t>(u) - static_cast<int64_t>(bias));
                return true;
            };
            std::vector<size_t> sizes;
            sizes.reserve(num_frames);
            sizes.push_back(static_cast<size_t>(first_size));
            for (uint8_t i = 1; i < num_frames - 1; ++i) {
                int64_t delta = 0;
                if (!read_signed_vint(delta))
                    return;
                int64_t sz = static_cast<int64_t>(sizes.back()) + delta;
                if (sz < 0)
                    return;
                sizes.push_back(static_cast<size_t>(sz));
            }
            // Last frame size is remainder
            size_t consumed = 0;
            for (size_t i = 0; i < sizes.size(); ++i)
                consumed += sizes[i];
            size_t remaining = block_end - cur.Tell();
            size_t last_sz = remaining - consumed;
            sizes.push_back(last_sz);
            for (size_t sz : sizes) {
                payloads.emplace_back(ReadPayload(cur, sz));
                payload_timestamps.push_back(0);
            }
        }

        auto it = tracks_.find(track_number);
        if (it == tracks_.end()) {
            statistics_["unknown_track_blocks"]++;
            return;
        }
        const TrackInfo &ti = it->second;
        uint64_t timestamp_ns = current_cluster_timecode_ns_ + static_cast<int64_t>(rel_tc) * timecode_scale_ns_;

        // Calculate per-frame timestamps for laced frames if DefaultDuration is known
        for (size_t i = 0; i < payloads.size(); ++i) {
            const auto &payload = payloads[i];
            std::vector<uint8_t> out;
            if (ti.track_type == kTrackTypeVideo && StartsWith(ti.codec_id, "V_MPEG4/ISO/AVC")) {
                out = ConvertAvccFrameToAnnexB(ti, payload.data(), payload.size(), keyframe);
            } else if (ti.track_type == kTrackTypeVideo && StartsWith(ti.codec_id, "V_MPEGH/ISO/HEVC")) {
                out = ConvertHvccFrameToAnnexB(ti, payload.data(), payload.size(), keyframe);
            } else if (ti.track_type == kTrackTypeAudio && StartsWith(ti.codec_id, "A_AAC")) {
                auto adts = BuildAdtsHeader(ti, payload.size());
                out.insert(out.end(), adts.begin(), adts.end());
                out.insert(out.end(), payload.begin(), payload.end());
            } else if (ti.track_type == kTrackTypeAudio && StartsWith(ti.codec_id, "A_OPUS")) {
                // For Opus, emit raw Opus packets (no Ogg framing) and let consumer wrap if needed.
                out.insert(out.end(), payload.begin(), payload.end());
            } else {
                statistics_["unsupported_codec_frames"]++;
                continue;
            }
            if (listener_ && !out.empty()) {
                if (!track_filter_.empty() && track_filter_.count(track_number) == 0) {
                    continue;
                }
                uint64_t ts_emit = timestamp_ns;
                if (i > 0 && ti.default_duration_ns > 0) {
                    ts_emit = timestamp_ns + static_cast<uint64_t>(i) * ti.default_duration_ns;
                }
                MkvFrame f;
                f.track_number = track_number;
                f.timecode_ns = static_cast<int64_t>(ts_emit);
                f.keyframe = keyframe;
                f.data = out.data();
                f.size = out.size();
                listener_->OnFrame(f);
            }
        }
    }

private:
    mutable std::mutex mutex_;
    bool running_;
    uint64_t timecode_scale_ns_;
    uint64_t current_cluster_timecode_ns_;

    std::unordered_map<uint64_t, TrackInfo> tracks_;
    std::unordered_set<uint64_t> track_filter_;
    std::unordered_map<std::string, uint64_t> statistics_;
    // Legacy callbacks removed; use class-based listener only.
    IMkvDemuxListener *listener_{nullptr};
};

// MkvDemuxer public API
MkvDemuxer::MkvDemuxer() : impl_(new Impl) {}
MkvDemuxer::~MkvDemuxer() = default;
MkvDemuxer::MkvDemuxer(MkvDemuxer &&) noexcept = default;
MkvDemuxer &MkvDemuxer::operator=(MkvDemuxer &&) noexcept = default;

// Legacy function-based callbacks removed.

void MkvDemuxer::SetTrackFilter(const std::vector<uint64_t> &tracks)
{
    impl_->SetTrackFilter(tracks);
}

bool MkvDemuxer::Start()
{
    return impl_->Start();
}
void MkvDemuxer::Stop()
{
    impl_->Stop();
}
bool MkvDemuxer::IsRunning() const
{
    return impl_->IsRunning();
}

// Removed legacy ParseData/DemuxInput public APIs

size_t MkvDemuxer::Consume(const uint8_t *data, size_t size, bool /*end_of_stream*/)
{
    // Minimal streaming: delegate to internal ParseData which adapts buffer to Input.
    return impl_->ParseData(data, size);
}

void MkvDemuxer::SetListener(IMkvDemuxListener *listener)
{
    impl_->SetListener(listener);
}

std::unordered_map<std::string, uint64_t> MkvDemuxer::GetStatistics() const
{
    return impl_->GetStatistics();
}
void MkvDemuxer::ResetStatistics()
{
    impl_->ResetStatistics();
}
void MkvDemuxer::Reset()
{
    impl_->Reset();
}

} // namespace lmshao::lmmkv