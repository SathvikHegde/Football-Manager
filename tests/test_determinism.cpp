#include "test_framework.hpp"
#include "fm/math/rng.hpp"
#include "fm/model/match_state.hpp"
#include "fm/engine/match_simulator.hpp"

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

    state1.rng = fm::RngState(42);
    state2.rng = fm::RngState(42);

    for (int tick = 0; tick < 100; ++tick) {
        sim1.step_tick(state1);
        sim2.step_tick(state2);

        REQUIRE(state1.tick == state2.tick);
        REQUIRE(state1.score_home == state2.score_home);
        REQUIRE(state1.score_away == state2.score_away);
        REQUIRE(state1.ball.position == state2.ball.position);
        REQUIRE(state1.rng.state == state2.rng.state);
    }
}

