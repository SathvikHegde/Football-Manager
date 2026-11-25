#include "fm/recording/match_recording.hpp"
#include <algorithm>

namespace fm {

void MatchRecording::reserve(std::size_t num_frames) {
    frames_.reserve(num_frames);
    frame_meta_.reserve(num_frames);
}

void MatchRecording::add_frame(const FrameSnapshot& snapshot) {
    frames_.push_back(snapshot);
    frame_meta_.push_back(FrameMeta{});
}

void MatchRecording::add_frame(const FrameSnapshot& snapshot, const MatchState& state) {
    frames_.push_back(snapshot);
    frame_meta_.push_back(FrameMeta{
        state.play_state,
        state.half,
        state.score_home,
        state.score_away
    });
}

void MatchRecording::add_events(const std::vector<MatchEvent>& new_events) {
    events_.insert(events_.end(), new_events.begin(), new_events.end());
}

void MatchRecording::truncate(std::size_t target_tick) {
    if (target_tick < frames_.size()) {
        frames_.resize(target_tick);
    }
    if (target_tick < frame_meta_.size()) {
        frame_meta_.resize(target_tick);
    }
    std::erase_if(events_, [target_tick](const MatchEvent& evt) {
        return static_cast<std::size_t>(evt.tick) > target_tick;
    });
}

void MatchRecording::clear() {
    frames_.clear();
    frame_meta_.clear();
    events_.clear();
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

const FrameMeta& MatchRecording::get_frame_meta(std::size_t tick) const {
    if (tick < frame_meta_.size()) {
        return frame_meta_[tick];
    }
    static const FrameMeta empty_meta{};
    return empty_meta;
}

} // namespace fm
