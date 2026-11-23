#include "test_framework.hpp"
#include "fm/recording/checkpoint_manager.hpp"
#include "fm/recording/frame_snapshot.hpp"
#include "fm/recording/match_recording.hpp"

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

