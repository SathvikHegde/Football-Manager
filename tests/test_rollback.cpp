#include "test_framework.hpp"
#include "fm/engine/match_simulator.hpp"
#include "fm/engine/pitch.hpp"
#include "fm/model/tactics.hpp"
#include "fm/recording/checkpoint_manager.hpp"
#include "fm/recording/frame_snapshot.hpp"
#include "fm/recording/match_recording.hpp"
#include <cstring>

namespace {

bool frames_byte_identical(const fm::MatchRecording& a,
                           const fm::MatchRecording& b,
                           std::size_t begin_idx,
                           std::size_t end_idx) {
    for (std::size_t t = begin_idx; t < end_idx; ++t) {
        const auto sa = a.get_frame_span(t);
        const auto sb = b.get_frame_span(t);
        if (std::memcmp(sa.data(), sb.data(), fm::FLOATS_PER_FRAME * sizeof(float)) != 0) {
            return false;
        }
    }
    return true;
}

std::size_t count_divergent_frames(const fm::MatchRecording& a,
                                   const fm::MatchRecording& b,
                                   std::size_t begin_idx,
                                   std::size_t end_idx) {
    std::size_t count = 0;
    for (std::size_t t = begin_idx; t < end_idx; ++t) {
        const auto sa = a.get_frame_span(t);
        const auto sb = b.get_frame_span(t);
        if (std::memcmp(sa.data(), sb.data(), fm::FLOATS_PER_FRAME * sizeof(float)) != 0) {
            count++;
        }
    }
    return count;
}

} // namespace

TEST_CASE("Checkpoint Manager Rollback State Retrieval", "[rollback]") {
    fm::CheckpointManager mgr;

    fm::MatchState s0{};
    s0.tick = 0;
    mgr.save_checkpoint(s0);

    fm::MatchState s1200{};
    s1200.tick = 1200;
    mgr.save_checkpoint(s1200);

    fm::MatchState s2400{};
    s2400.tick = 2400;
    mgr.save_checkpoint(s2400);

    REQUIRE(mgr.count() == 3);
    REQUIRE(mgr.has_checkpoint_at(0));
    REQUIRE(mgr.has_checkpoint_at(1200));
    REQUIRE_FALSE(mgr.has_checkpoint_at(1500));

    // Rollback at tick 1500 should find checkpoint at tick 1200
    const auto* nearest = mgr.get_nearest_checkpoint(1500);
    REQUIRE(nearest != nullptr);
    REQUIRE(nearest->tick == 1200);

    // Rollback at tick 500 should find checkpoint at tick 0
    nearest = mgr.get_nearest_checkpoint(500);
    REQUIRE(nearest != nullptr);
    REQUIRE(nearest->tick == 0);

    // Truncate after tick 1200
    mgr.truncate_after(1200);
    REQUIRE(mgr.count() == 2);
    REQUIRE(mgr.get_nearest_checkpoint(3000)->tick == 1200);
}

TEST_CASE("Flat Frame Snapshot Serialization 93 Floats Wire Format", "[rollback]") {
    fm::MatchState state{};
    state.tick = 42;
    state.ball.position = fm::Vec3{1.0f, 2.0f, 3.0f};
    state.ball.owner_slot = 7;

    state.players[0].position = fm::Vec2{-50.0f, 0.0f};
    state.players[0].facing_rad = 1.57f;
    state.players[0].stamina_pct = 0.95f;

    fm::FrameSnapshot snap = fm::FrameSnapshot::from_state(state);

    REQUIRE(snap.data[0] == 42.0f);
    REQUIRE(snap.data[1] == 1.0f);
    REQUIRE(snap.data[2] == 2.0f);
    REQUIRE(snap.data[3] == 3.0f);
    REQUIRE(snap.data[4] == 7.0f);

    // Player 0 at index 5..8
    REQUIRE(snap.data[5] == -50.0f);
    REQUIRE(snap.data[6] == 0.0f);
    REQUIRE(snap.data[7] == 1.57f);
    REQUIRE(snap.data[8] == 0.95f);
}

TEST_CASE("Stage 3 DoD - Checkpoint Interval & Identical Minute-60 Rollback Byte-Identical", "[rollback]") {
    constexpr uint32_t MINUTE_60_TICK = 60u * 60u * 20u; // 72,000 ticks

    fm::MatchSimulator sim;
    fm::MatchState state{};
    fm::MatchRecording recording;
    fm::CheckpointManager checkpoints;

    sim.initialize_match(state, 0xDEADBEEFULL, fm::FormationId::F_442, fm::FormationId::F_433);
    sim.simulate_match(state, recording, checkpoints, fm::Pitch::TICKS_FULL_TIME);

    // Verify 90 checkpoints captured (every 1,200 ticks: 0, 1200, ..., 106800) within budget
    REQUIRE(recording.frame_count() == fm::Pitch::TICKS_FULL_TIME);
    REQUIRE(checkpoints.count() == 90);
    REQUIRE(checkpoints.has_checkpoint_at(0));
    REQUIRE(checkpoints.has_checkpoint_at(MINUTE_60_TICK));
    REQUIRE(checkpoints.has_checkpoint_at(106800));
    REQUIRE(checkpoints.memory_bytes() < 512u * 1024u);
    REQUIRE(checkpoints.get_nearest_checkpoint(MINUTE_60_TICK)->tick == MINUTE_60_TICK);
    REQUIRE(checkpoints.get_nearest_checkpoint(MINUTE_60_TICK + 500u)->tick == MINUTE_60_TICK);

    const fm::MatchRecording baseline_rec = recording;
    const fm::MatchState baseline_state = state;

    // Pause at minute 60, apply IDENTICAL tactical config (Home: 4-4-2, line 0.5, press 0.5), and resimulate
    const bool ok = sim.rollback_and_resimulate(
        state, recording, checkpoints, MINUTE_60_TICK,
        0, fm::FormationId::F_442, 0.5f, 0.5f, fm::Pitch::TICKS_FULL_TIME);

    REQUIRE(ok);
    REQUIRE(recording.frame_count() == fm::Pitch::TICKS_FULL_TIME);
    REQUIRE(checkpoints.count() == 90);
    REQUIRE(state == baseline_state);
    REQUIRE(recording.events().size() == baseline_rec.events().size());

    for (std::size_t e = 0; e < baseline_rec.events().size(); ++e) {
        REQUIRE(recording.events()[e] == baseline_rec.events()[e]);
    }

    REQUIRE(frames_byte_identical(recording, baseline_rec, 0, fm::Pitch::TICKS_FULL_TIME));
    for (std::size_t t = 0; t < fm::Pitch::TICKS_FULL_TIME; ++t) {
        REQUIRE(recording.get_frame_meta(t) == baseline_rec.get_frame_meta(t));
    }
}

TEST_CASE("Stage 3 DoD - Mutated Minute-60 Tactics (Low Block -> High Press) Diverges From Minute 60", "[rollback]") {
    constexpr uint32_t MINUTE_60_TICK = 60u * 60u * 20u; // 72,000 ticks

    fm::MatchSimulator sim;
    fm::MatchState state{};
    fm::MatchRecording recording;
    fm::CheckpointManager checkpoints;

    // Start Home in a deep low block (5-3-2, defensive_line=0.15, pressing=0.15)
    sim.initialize_match(state, 0xDEADBEEFULL, fm::FormationId::F_532, fm::FormationId::F_433);
    fm::apply_tactical_change(state, 0, fm::FormationId::F_532, 0.15f, 0.15f);
    sim.simulate_match(state, recording, checkpoints, fm::Pitch::TICKS_FULL_TIME);

    REQUIRE(recording.frame_count() == fm::Pitch::TICKS_FULL_TIME);
    const fm::MatchRecording low_block_rec = recording;

    // Pause at minute 60, switch Home to high press 4-3-3 (defensive_line=0.90, pressing=0.95), resimulate
    const bool ok = sim.rollback_and_resimulate(
        state, recording, checkpoints, MINUTE_60_TICK,
        0, fm::FormationId::F_433, 0.90f, 0.95f, fm::Pitch::TICKS_FULL_TIME);

    REQUIRE(ok);
    REQUIRE(recording.frame_count() == fm::Pitch::TICKS_FULL_TIME);
    REQUIRE(checkpoints.count() == 90);

    // Pre-minute-60 frames [0 .. 71999] must remain 100% byte-identical
    REQUIRE(frames_byte_identical(recording, low_block_rec, 0, MINUTE_60_TICK));

    // Post-minute-60 frames [72000 .. 107999] must diverge meaningfully
    const std::size_t divergent = count_divergent_frames(
        recording, low_block_rec, MINUTE_60_TICK, fm::Pitch::TICKS_FULL_TIME);
    REQUIRE(divergent > 30000);

    // Verify tactical metadata & TacticalChange event at minute 60
    const auto& post_meta = recording.get_frame_meta(MINUTE_60_TICK);
    REQUIRE(post_meta.home_formation == fm::FormationId::F_433);
    REQUIRE(post_meta.home_def_line_pct == 90);
    REQUIRE(post_meta.home_press_pct == 95);

    bool found_tactical_evt = false;
    for (const auto& evt : recording.events()) {
        if (evt.type == fm::EventType::TacticalChange && evt.tick == MINUTE_60_TICK) {
            found_tactical_evt = true;
            REQUIRE(evt.primary_slot == 0);
            REQUIRE(evt.secondary_slot == static_cast<int8_t>(fm::FormationId::F_433));
        }
    }
    REQUIRE(found_tactical_evt);
}

TEST_CASE("Mid-Checkpoint Rollback & Multi-Edit History Consistency", "[rollback]") {
    constexpr uint32_t TEST_LIMIT = 40000u; // > 33 minutes, crosses multiple 1,200-tick checkpoints
    constexpr uint32_t TICK_EDIT_A = 36500u; // Between checkpoints 36000 and 37200
    constexpr uint32_t TICK_EDIT_B = 36800u; // Also between checkpoints 36000 and 37200
    constexpr uint32_t TICK_EARLY  = 24000u; // Prior to both edits

    fm::MatchSimulator sim;
    fm::MatchState state{};
    fm::MatchRecording recording;
    fm::CheckpointManager checkpoints;

    sim.initialize_match(state, 0xDEADBEEFULL, fm::FormationId::F_442, fm::FormationId::F_433);
    sim.simulate_match(state, recording, checkpoints, TEST_LIMIT);
    REQUIRE(recording.frame_count() == TEST_LIMIT);

    const fm::MatchRecording baseline_rec = recording;

    // 1. Mid-checkpoint rollback at 36500 with identical tactics -> 100% byte-identical
    REQUIRE(sim.rollback_and_resimulate(
        state, recording, checkpoints, TICK_EDIT_A,
        0, fm::FormationId::F_442, 0.5f, 0.5f, TEST_LIMIT));
    REQUIRE(frames_byte_identical(recording, baseline_rec, 0, TEST_LIMIT));

    // 2. Edit A at 36500 (Home -> 4-3-3 high press)
    REQUIRE(sim.rollback_and_resimulate(
        state, recording, checkpoints, TICK_EDIT_A,
        0, fm::FormationId::F_433, 0.85f, 0.90f, TEST_LIMIT));
    const fm::MatchRecording edit_a_rec = recording;
    REQUIRE(frames_byte_identical(edit_a_rec, baseline_rec, 0, TICK_EDIT_A));
    REQUIRE(count_divergent_frames(edit_a_rec, baseline_rec, TICK_EDIT_A, TEST_LIMIT) > 3000);

    // 3. Edit B at 36800 (Away -> 5-3-2 low block): fast-forwards 36000 -> 36800 replaying Edit A at 36500!
    REQUIRE(sim.rollback_and_resimulate(
        state, recording, checkpoints, TICK_EDIT_B,
        1, fm::FormationId::F_532, 0.20f, 0.20f, TEST_LIMIT));
    REQUIRE(frames_byte_identical(recording, baseline_rec, 0, TICK_EDIT_A));
    REQUIRE(frames_byte_identical(recording, edit_a_rec, 0, TICK_EDIT_B));
    REQUIRE(count_divergent_frames(recording, edit_a_rec, TICK_EDIT_B, TEST_LIMIT) > 2500);

    // 4. Roll back earlier to 24000 -> discards abandoned future edits (36500 & 36800) cleanly
    REQUIRE(sim.rollback_and_resimulate(state, recording, checkpoints, TICK_EARLY, TEST_LIMIT));
    REQUIRE(frames_byte_identical(recording, baseline_rec, 0, TEST_LIMIT));
}

