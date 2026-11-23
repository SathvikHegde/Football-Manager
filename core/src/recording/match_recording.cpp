#include "fm/recording/match_recording.hpp"
#include <stdexcept>

namespace fm {

void MatchRecording::reserve(std::size_t num_frames) {
    frames_.reserve(num_frames);
}

void MatchRecording::add_frame(const FrameSnapshot& snapshot) {
    frames_.push_back(snapshot);
}

void MatchRecording::truncate(std::size_t target_tick) {
    if (target_tick < frames_.size()) {
        frames_.resize(target_tick);
    }
}

void MatchRecording::clear() {
    frames_.clear();
}

const FrameSnapshot& MatchRecording::get_frame(std::size_t tick) const {
    if (tick < frames_.size()) {
        return frames_[tick];
    }
    static const FrameSnapshot empty{};
    return empty;
}

std::span<const float> MatchRecording::get_frame_span(std::size_t tick) const {
    const auto& frame = get_frame(tick);
    return std::span<const float>(frame.data.data(), FLOATS_PER_FRAME);
}

} // namespace fm

