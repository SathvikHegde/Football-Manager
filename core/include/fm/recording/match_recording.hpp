#pragma once
#include <vector>
#include <span>
#include <cstddef>
#include <cstdint>
#include "fm/model/match_event.hpp"
#include "fm/model/match_state.hpp"
#include "fm/recording/frame_snapshot.hpp"

namespace fm {

struct FrameMeta {
    PlayState play_state{PlayState::Kickoff};
    uint8_t half{1};
    uint8_t score_home{0};
    uint8_t score_away{0};
    FormationId home_formation{FormationId::F_442};
    FormationId away_formation{FormationId::F_442};
    uint8_t home_def_line_pct{50};
    uint8_t home_press_pct{50};
    uint8_t away_def_line_pct{50};
    uint8_t away_press_pct{50};

    [[nodiscard]] constexpr bool operator==(const FrameMeta& o) const = default;
};

class MatchRecording {
public:
    MatchRecording() = default;

    void reserve(std::size_t num_frames);
    void add_frame(const FrameSnapshot& snapshot);
    void add_frame(const FrameSnapshot& snapshot, const MatchState& state);
    void add_events(const std::vector<MatchEvent>& new_events);
    void truncate(std::size_t target_tick);
    void clear();

    [[nodiscard]] std::size_t frame_count() const { return frames_.size(); }
    [[nodiscard]] const FrameSnapshot& get_frame(std::size_t tick) const;
    [[nodiscard]] std::span<const float> get_frame_span(std::size_t tick) const;
    [[nodiscard]] const FrameMeta& get_frame_meta(std::size_t tick) const;
    [[nodiscard]] const std::vector<MatchEvent>& events() const { return events_; }

private:
    std::vector<FrameSnapshot> frames_{};
    std::vector<FrameMeta> frame_meta_{};
    std::vector<MatchEvent> events_{};
};

} // namespace fm
