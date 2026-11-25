#pragma once
#include <array>
#include <cstdint>
#include <cstddef>
#include "fm/model/match_state.hpp"

namespace fm {

constexpr std::size_t FLOATS_PER_FRAME = 93;

struct FrameSnapshot {
    std::array<float, FLOATS_PER_FRAME> data{};

    static FrameSnapshot from_state(const MatchState& state);
    void write_to(float* dest) const;

    [[nodiscard]] constexpr bool operator==(const FrameSnapshot& o) const = default;
};

} // namespace fm

