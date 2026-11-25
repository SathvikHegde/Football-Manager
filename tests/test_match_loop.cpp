#include "test_framework.hpp"
#include "fm/engine/match_simulator.hpp"
#include "fm/engine/pitch.hpp"
#include "fm/model/match_event.hpp"
#include "fm/recording/match_recording.hpp"
#include <cmath>
#include <array>

TEST_CASE("Stage 2 DoD - Full 90-Minute Match (108000 Ticks) Without Deadlocks or State Locks", "[match_loop]") {
    fm::MatchSimulator sim;
    fm::MatchState state{};
    fm::MatchRecording recording;

    sim.initialize_match(state, 12345ULL, fm::FormationId::F_442, fm::FormationId::F_433);
    sim.simulate_match(state, recording, fm::Pitch::TICKS_FULL_TIME);

    REQUIRE(recording.frame_count() == fm::Pitch::TICKS_FULL_TIME);
    REQUIRE(state.tick == fm::Pitch::TICKS_FULL_TIME);
    REQUIRE(state.half == 2);
    REQUIRE(state.play_state == fm::PlayState::FullTime);

    int open_play_frames = 0;
    int consecutive_non_open_frames = 0;
    int max_consecutive_non_open = 0;

    for (std::size_t t = 0; t < recording.frame_count(); ++t) {
        const auto span = recording.get_frame_span(t);
        const auto& meta = recording.get_frame_meta(t);

        REQUIRE(span.size() == fm::FLOATS_PER_FRAME);

        // Verify ball finite and within bounds
        REQUIRE(std::isfinite(span[1]));
        REQUIRE(std::isfinite(span[2]));
        REQUIRE(std::isfinite(span[3]));
        REQUIRE(span[1] >= -56.0f);
        REQUIRE(span[1] <= 56.0f);
        REQUIRE(span[2] >= -37.0f);
        REQUIRE(span[2] <= 37.0f);
        REQUIRE(span[3] >= 0.0f);
        REQUIRE(span[3] <= 35.0f);

        // Verify all 22 players remain valid and bounded
        for (std::size_t p = 0; p < 22; ++p) {
            const std::size_t base = 5 + p * 4;
            REQUIRE(std::isfinite(span[base + 0]));
            REQUIRE(std::isfinite(span[base + 1]));
            REQUIRE(std::isfinite(span[base + 2]));
            REQUIRE(std::isfinite(span[base + 3]));

            REQUIRE(span[base + 0] >= -55.0f);
            REQUIRE(span[base + 0] <= 55.0f);
            REQUIRE(span[base + 1] >= -36.5f);
            REQUIRE(span[base + 1] <= 36.5f);
            REQUIRE(span[base + 3] >= 0.14f);
            REQUIRE(span[base + 3] <= 1.0f);
        }

        if (meta.play_state == fm::PlayState::OpenPlay) {
            open_play_frames++;
            consecutive_non_open_frames = 0;
        } else if (meta.play_state != fm::PlayState::FullTime) {
            consecutive_non_open_frames++;
            if (consecutive_non_open_frames > max_consecutive_non_open) {
                max_consecutive_non_open = consecutive_non_open_frames;
            }
        }
    }

    // Majority of match is active open play and no restart/set-piece ever locks
    REQUIRE(open_play_frames > 90000);
    REQUIRE(max_consecutive_non_open <= 80);

    // Verify all core match behaviors occur during the 90 minutes
    int kickoffs = 0;
    int passes = 0;
    int pass_complete = 0;
    int interceptions = 0;
    int tackles = 0;
    int dribbles = 0;
    int shots = 0;
    int halftimes = 0;
    int fulltimes = 0;

    for (const auto& evt : recording.events()) {
        switch (evt.type) {
            case fm::EventType::Kickoff:         kickoffs++; break;
            case fm::EventType::Pass:            passes++; break;
            case fm::EventType::PassComplete:    pass_complete++; break;
            case fm::EventType::PassIntercepted: interceptions++; break;
            case fm::EventType::TackleWon:       tackles++; break;
            case fm::EventType::Dribble:         dribbles++; break;
            case fm::EventType::Shot:            shots++; break;
            case fm::EventType::HalfTime:        halftimes++; break;
            case fm::EventType::FullTime:        fulltimes++; break;
            default: break;
        }
    }

    REQUIRE(kickoffs >= 2);
    REQUIRE(halftimes == 1);
    REQUIRE(fulltimes == 1);
    REQUIRE(passes >= 100);
    REQUIRE(pass_complete >= 50);
    REQUIRE(dribbles >= 10);
    REQUIRE((tackles + interceptions) >= 20);
    REQUIRE(shots >= 6);
}

TEST_CASE("Stage 2 DoD - Multi-Seed Match Stability & Tactical Formation Variety", "[match_loop]") {
    const std::array<uint64_t, 3> seeds = {101ULL, 2026ULL, 0xCAFEBABEULL};
    const std::array<fm::FormationId, 3> home_forms = {fm::FormationId::F_442, fm::FormationId::F_4231, fm::FormationId::F_352};
    const std::array<fm::FormationId, 3> away_forms = {fm::FormationId::F_433, fm::FormationId::F_532, fm::FormationId::F_442};

    for (std::size_t idx = 0; idx < seeds.size(); ++idx) {
        fm::MatchSimulator sim;
        fm::MatchState state{};
        fm::MatchRecording recording;

        sim.initialize_match(state, seeds[idx], home_forms[idx], away_forms[idx]);
        sim.simulate_match(state, recording, fm::Pitch::TICKS_FULL_TIME);

        REQUIRE(recording.frame_count() == fm::Pitch::TICKS_FULL_TIME);
        REQUIRE(state.play_state == fm::PlayState::FullTime);
        REQUIRE(std::isfinite(state.ball.position.x));
        REQUIRE(std::isfinite(state.ball.position.y));
        REQUIRE(std::isfinite(state.ball.position.z));
    }
}

