#include "test_framework.hpp"
#include "fm/math/rng.hpp"
#include "fm/model/match_state.hpp"
#include "fm/engine/match_simulator.hpp"
#include "fm/engine/pitch.hpp"
#include "fm/recording/checkpoint_manager.hpp"
#include "fm/recording/match_recording.hpp"
#include <cstring>

TEST_CASE("PRNG Determinism - Identical Seeds Produce Identical Sequences", "[determinism]") {
    const uint64_t seed = 0xDEADBEEFULL;

    fm::RngState rng1(seed);
    fm::RngState rng2(seed);

    for (int i = 0; i < 1000; ++i) {
        uint32_t val1 = rng1.next_u32();
        uint32_t val2 = rng2.next_u32();
        REQUIRE(val1 == val2);

        float f1 = rng1.next_float();
        float f2 = rng2.next_float();
        REQUIRE(f1 == f2);
        REQUIRE(f1 >= 0.0f);
        REQUIRE(f1 < 1.0f);

        float g1 = rng1.next_gaussian(0.0f, 1.0f);
        float g2 = rng2.next_gaussian(0.0f, 1.0f);
        REQUIRE(g1 == g2);
    }
}

TEST_CASE("Simulation Tick Determinism Across Runs", "[determinism]") {
    fm::MatchSimulator sim1;
    fm::MatchSimulator sim2;

    fm::MatchState state1{};
    fm::MatchState state2{};

    sim1.initialize_match(state1, 42ULL);
    sim2.initialize_match(state2, 42ULL);

    for (int tick = 0; tick < 500; ++tick) {
        sim1.step_tick(state1);
        sim2.step_tick(state2);

        REQUIRE(state1.tick == state2.tick);
        REQUIRE(state1.score_home == state2.score_home);
        REQUIRE(state1.score_away == state2.score_away);
        REQUIRE(state1.ball.position == state2.ball.position);
        REQUIRE(state1.rng.state == state2.rng.state);
    }
}

TEST_CASE("Stage 3 DoD - Full Match Bit-Identical Determinism Across 108000 Frames (0xDEADBEEF)", "[determinism]") {
    fm::MatchSimulator sim1;
    fm::MatchSimulator sim2;
    fm::MatchState state1{};
    fm::MatchState state2{};
    fm::MatchRecording rec1;
    fm::MatchRecording rec2;
    fm::CheckpointManager cp1;
    fm::CheckpointManager cp2;

    sim1.initialize_match(state1, 0xDEADBEEFULL, fm::FormationId::F_442, fm::FormationId::F_433);
    sim2.initialize_match(state2, 0xDEADBEEFULL, fm::FormationId::F_442, fm::FormationId::F_433);

    sim1.simulate_match(state1, rec1, cp1, fm::Pitch::TICKS_FULL_TIME);
    sim2.simulate_match(state2, rec2, cp2, fm::Pitch::TICKS_FULL_TIME);

    REQUIRE(rec1.frame_count() == fm::Pitch::TICKS_FULL_TIME);
    REQUIRE(rec2.frame_count() == fm::Pitch::TICKS_FULL_TIME);
    REQUIRE(cp1.count() == 90);
    REQUIRE(cp2.count() == 90);
    REQUIRE(state1 == state2);
    REQUIRE(rec1.events().size() == rec2.events().size());

    for (std::size_t e = 0; e < rec1.events().size(); ++e) {
        REQUIRE(rec1.events()[e] == rec2.events()[e]);
    }

    for (std::size_t t = 0; t < rec1.frame_count(); ++t) {
        const auto s1 = rec1.get_frame_span(t);
        const auto s2 = rec2.get_frame_span(t);
        REQUIRE(s1.size() == fm::FLOATS_PER_FRAME);
        REQUIRE(s2.size() == fm::FLOATS_PER_FRAME);
        REQUIRE(std::memcmp(s1.data(), s2.data(), fm::FLOATS_PER_FRAME * sizeof(float)) == 0);
        for (std::size_t k = 0; k < fm::FLOATS_PER_FRAME; ++k) {
            REQUIRE(s1[k] == s2[k]);
        }
        REQUIRE(rec1.get_frame_meta(t) == rec2.get_frame_meta(t));
    }
}

TEST_CASE("Distinct Seeds Produce Divergent Match Simulations", "[determinism]") {
    fm::MatchSimulator sim1;
    fm::MatchSimulator sim2;
    fm::MatchState state1{};
    fm::MatchState state2{};
    fm::MatchRecording rec1;
    fm::MatchRecording rec2;

    sim1.initialize_match(state1, 0xDEADBEEFULL);
    sim2.initialize_match(state2, 0xCAFEBABEULL);

    sim1.simulate_match(state1, rec1, 2400);
    sim2.simulate_match(state2, rec2, 2400);

    REQUIRE(rec1.frame_count() == 2400);
    REQUIRE(rec2.frame_count() == 2400);

    int diff_frames = 0;
    for (std::size_t t = 0; t < 2400; ++t) {
        if (std::memcmp(rec1.get_frame(t).data.data(),
                        rec2.get_frame(t).data.data(),
                        fm::FLOATS_PER_FRAME * sizeof(float)) != 0) {
            diff_frames++;
        }
    }
    REQUIRE(diff_frames > 1000);
}
