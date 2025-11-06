/**
 * @author SHAO Liming <lmshao@163.com>
 * @copyright Copyright (c) 2025 SHAO Liming
 * @license MIT
 *
 * SPDX-License-Identifier: MIT
 */

#include "lmmkv/mkv_muxer.h"

namespace lmshao::lmmkv {

struct MkvMuxer::Impl {
    MkvMuxerOptions opts;
    IMkvMuxListener *listener = nullptr;
    void *writer = nullptr; // examples/utilities may provide a concrete writer
    MkvInfo info;
    std::vector<MkvTrackInfo> tracks;

    explicit Impl(const MkvMuxerOptions &o) : opts(o) {}

    void ResetInternal()
    {
        tracks.clear();
        info = MkvInfo{};
    }
};

MkvMuxer::MkvMuxer(const MkvMuxerOptions &opts) : impl_(new Impl(opts)) {}
MkvMuxer::~MkvMuxer() = default;

MkvMuxer::MkvMuxer(MkvMuxer &&) noexcept = default;
MkvMuxer &MkvMuxer::operator=(MkvMuxer &&) noexcept = default;

void MkvMuxer::SetListener(IMkvMuxListener *listener)
{
    impl_->listener = listener;
}
void MkvMuxer::SetWriter(void *writer)
{
    impl_->writer = writer;
}

bool MkvMuxer::AddTrack(const MkvTrackInfo &track)
{
    impl_->tracks.push_back(track);
    if (impl_->listener)
        impl_->listener->OnTrackWritten(track);
    return true;
}

bool MkvMuxer::BeginSegment(const MkvInfo &info)
{
    impl_->info = info;
    if (impl_->listener)
        impl_->listener->OnSegmentStart();
    // TODO: write EBML header + Segment + Info + Tracks
    return true;
}

bool MkvMuxer::WriteFrame(const MkvFrame &frame)
{
    (void)frame;
    // TODO: rolling Cluster management and SimpleBlock writing
    return impl_->writer != nullptr;
}

bool MkvMuxer::EndSegment()
{
    // TODO: optionally write Cues for seekable outputs
    return true;
}

void MkvMuxer::Reset()
{
    impl_->ResetInternal();
}

} // namespace lmshao::lmmkv