#include "fm/recording/frame_snapshot.hpp"
#include <cstring>

namespace fm {

FrameSnapshot FrameSnapshot::from_state(const MatchState& state) {
    FrameSnapshot snap{};

    // [0]: Frame Tick (float)
    snap.data[0] = static_cast<float>(state.tick);
    // [1..3]: Ball Position X, Y, Z
    snap.data[1] = state.ball.position.x;
    snap.data[2] = state.ball.position.y;
    snap.data[3] = state.ball.position.z;
    // [4]: Ball Owner Slot Index (-1.0 if loose)
    snap.data[4] = static_cast<float>(state.ball.owner_slot);

    // [5 + i*4 + 0..3]: Player i
    for (std::size_t i = 0; i < 22; ++i) {
        std::size_t base = 5 + i * 4;
        const auto& player = state.players[i];
        snap.data[base + 0] = player.position.x;
        snap.data[base + 1] = player.position.y;
        snap.data[base + 2] = player.facing_rad;
        snap.data[base + 3] = player.stamina_pct;
    }

    return snap;
}

void FrameSnapshot::write_to(float* dest) const {
    if (dest) {
        std::memcpy(dest, data.data(), FLOATS_PER_FRAME * sizeof(float));
    }
}

} // namespace fm

