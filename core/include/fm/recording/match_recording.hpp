#pragma once
#include <vector>
#include <span>
#include <cstddef>
#include "fm/recording/frame_snapshot.hpp"

namespace fm {

class MatchRecording {
public:
    MatchRecording() = default;

    void reserve(std::size_t num_frames);
    void add_frame(const FrameSnapshot& snapshot);
    void truncate(std::size_t target_tick);
    void clear();

    [[nodiscard]] std::size_t frame_count() const { return frames_.size(); }
    [[nodiscard]] const FrameSnapshot& get_frame(std::size_t tick) const;
    [[nodiscard]] std::span<const float> get_frame_span(std::size_t tick) const;

private:
    std::vector<FrameSnapshot> frames_{};
};

} // namespace fm

