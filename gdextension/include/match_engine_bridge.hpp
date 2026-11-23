#pragma once

#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/variant/packed_float32_array.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/string.hpp>

#include <fm/engine/match_simulator.hpp>
#include <fm/recording/match_recording.hpp>
#include <fm/recording/checkpoint_manager.hpp>

namespace godot {

class MatchEngineBridge : public Node {
    GDCLASS(MatchEngineBridge, Node)

private:
    fm::MatchSimulator simulator_;
    fm::MatchState current_state_;
    fm::MatchRecording recording_;
    fm::CheckpointManager checkpoint_mgr_;
    uint64_t initial_seed_{12345};

protected:
    static void _bind_methods();

public:
    MatchEngineBridge();
    ~MatchEngineBridge() override;

    // Stage 0 Hello-World API
    [[nodiscard]] String ping() const;
    [[nodiscard]] String get_engine_version() const;

    void initialize_match(uint64_t seed);
    void simulate_full_match();

    // Low-level buffer access for Godot visualization
    [[nodiscard]] PackedFloat32Array get_frame_data(int tick) const;
    [[nodiscard]] Dictionary get_match_metadata() const;
    [[nodiscard]] int get_total_ticks() const;

    // Tactical Mutators & Snapshot Rollback API
    void apply_tactical_change(int team_id, int formation_id, float defensive_line, float pressing);
    void rollback_and_resimulate(int target_tick);
};

} // namespace godot

