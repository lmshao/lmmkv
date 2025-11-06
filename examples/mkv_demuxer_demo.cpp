/**
 * @author SHAO Liming <lmshao@163.com>
 * @copyright Copyright (c) 2025 SHAO Liming
 * @license MIT
 *
 * SPDX-License-Identifier: MIT
 */

#include <stdint.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <cstdio>
#include <cstring>
#include <fstream>
#include <map>
#include <memory>
#include <set>
#include <string>

#include "lmcore/mapped_file.h"
#include "lmmkv/lmmkv_logger.h"
#include "lmmkv/mkv_demuxer.h"

using namespace lmshao::lmmkv;

// Buffer-only API: read memory and feed demuxer

static std::set<uint64_t> ParseTrackList(const std::string &arg)
{
    std::set<uint64_t> out;
    size_t start = 0;
    while (start < arg.size()) {
        size_t comma = arg.find(',', start);
        std::string tok = (comma == std::string::npos) ? arg.substr(start) : arg.substr(start, comma - start);
        if (!tok.empty()) {
            try {
                uint64_t v = std::stoull(tok);
                out.insert(v);
            } catch (...) {
            }
        }
        if (comma == std::string::npos)
            break;
        start = comma + 1;
    }
    return out;
}

int main(int argc, char **argv)
{
    if (argc < 2) {
        std::fprintf(stderr, "Usage: %s <input.mkv> [--tracks=N1,N2,...] [--outdir=DIR]\n", argv[0]);
        return 1;
    }

    InitLmmkvLogger(lmshao::lmcore::LogLevel::kInfo);

    std::string input_path = argv[1];
    std::set<uint64_t> track_filter_set;
    std::string outdir = ".";
    for (int i = 2; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg.rfind("--tracks=", 0) == 0) {
            track_filter_set = ParseTrackList(arg.substr(9));
        } else if (arg.rfind("--outdir=", 0) == 0) {
            outdir = arg.substr(9);
        }
    }

    // Ensure output directory exists
    if (mkdir(outdir.c_str(), 0755) != 0) {
        // ignore EEXIST
    }
    auto mf = lmshao::lmcore::MappedFile::Open(input_path);
    if (!mf || !mf->IsValid()) {
        std::fprintf(stderr, "Failed to open: %s\n", input_path.c_str());
        return 1;
    }
    const uint8_t *data = mf->Data();
    size_t size = mf->Size();

    // Per-track output files
    std::map<uint64_t, std::ofstream> outputs;

    MkvDemuxer demuxer;
    // Listener-based callbacks
    struct DemoListener : public IMkvDemuxListener {
        std::map<uint64_t, std::ofstream> &outputs;
        std::string outdir;
        explicit DemoListener(std::map<uint64_t, std::ofstream> &o, std::string d) : outputs(o), outdir(std::move(d)) {}
        void OnInfo(const MkvInfo &info) override
        {
            printf("Info: timecode_scale=%llu ns, duration=%.3f s\n", (unsigned long long)info.timecode_scale_ns,
                   info.duration_seconds);
        }
        void OnTrack(const MkvTrackInfo &track) override
        {
            printf("Track %llu codec %s\n", (unsigned long long)track.track_number, track.codec_id.c_str());
            std::string base = outdir + "/track-" + std::to_string(track.track_number);
            std::string ext = ".bin";
            if (track.codec_id.rfind("V_MPEG4/ISO/AVC", 0) == 0)
                ext = ".h264";
            else if (track.codec_id.rfind("V_MPEGH/ISO/HEVC", 0) == 0)
                ext = ".h265";
            else if (track.codec_id.rfind("A_AAC", 0) == 0)
                ext = ".aac";
            else if (track.codec_id.rfind("A_OPUS", 0) == 0)
                ext = ".opus";
            std::string path = base + ext;
            outputs[track.track_number].open(path, std::ios::binary);
            if (!outputs[track.track_number].is_open()) {
                std::fprintf(stderr, "Failed to open output file: %s\n", path.c_str());
            } else {
                printf("Opened output: %s\n", path.c_str());
            }
        }
        void OnFrame(const MkvFrame &frame) override
        {
            auto it = outputs.find(frame.track_number);
            if (it == outputs.end())
                return;
            if (!it->second.is_open())
                return;
            it->second.write(reinterpret_cast<const char *>(frame.data), static_cast<std::streamsize>(frame.size));
        }
        void OnEndOfStream() override
        {
            printf("End of stream.\n");
            for (auto &kv : outputs) {
                if (kv.second.is_open())
                    kv.second.flush();
            }
        }
        void OnError(int code, const std::string &msg) override
        {
            std::fprintf(stderr, "Error(%d): %s\n", code, msg.c_str());
        }
    } listener(outputs, outdir);

    // Apply track filter if requested
    if (!track_filter_set.empty()) {
        std::vector<uint64_t> tracks(track_filter_set.begin(), track_filter_set.end());
        demuxer.SetTrackFilter(tracks);
    }

    if (!demuxer.Start()) {
        std::fprintf(stderr, "Demuxer failed to start\n");
        return 1;
    }

    demuxer.SetListener(&listener);
    (void)demuxer.Consume(data, size, true);
    demuxer.Stop();

    for (auto &kv : outputs) {
        if (kv.second.is_open())
            kv.second.close();
    }

    // Consume returns bytes processed; listener handles errors.

    printf("Demux finished.\n");
    return 0;
}