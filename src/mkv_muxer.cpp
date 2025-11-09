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
    MkvMuxerOptions opts_;
    IMkvMuxListener *listener_ = nullptr;
    void *writer_ = nullptr; // examples/utilities may provide a concrete writer
    MkvInfo info_;
    std::vector<MkvTrackInfo> tracks_;

    explicit Impl(const MkvMuxerOptions &o) : opts_(o) {}

    void ResetInternal()
    {
        tracks_.clear();
        info_ = MkvInfo{};
    }
};

MkvMuxer::MkvMuxer(const MkvMuxerOptions &opts) : impl_(new Impl(opts)) {}
MkvMuxer::~MkvMuxer() = default;

// move operations disabled in header

void MkvMuxer::SetListener(IMkvMuxListener *listener)
{
    impl_->listener_ = listener;
}
void MkvMuxer::SetWriter(void *writer)
{
    impl_->writer_ = writer;
}

bool MkvMuxer::AddTrack(const MkvTrackInfo &track)
{
    impl_->tracks_.push_back(track);
    if (impl_->listener_)
        impl_->listener_->OnTrackWritten(track);
    return true;
}

bool MkvMuxer::BeginSegment(const MkvInfo &info)
{
    impl_->info_ = info;
    if (impl_->listener_)
        impl_->listener_->OnSegmentStart();
    // TODO: write EBML header + Segment + Info + Tracks
    return true;
}

bool MkvMuxer::WriteFrame(const MkvFrame &frame)
{
    (void)frame;
    // TODO: rolling Cluster management and SimpleBlock writing
    return impl_->writer_ != nullptr;
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