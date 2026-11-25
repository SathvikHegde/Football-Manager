#pragma once

#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/variant/array.hpp>
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
    struct PendingTacticalEdit {
        bool active{false};
        fm::FormationId formation{fm::FormationId::F_442};
        float defensive_line{0.5f};
        float pressing_intensity{0.5f};
        bool has_full_extras{false};
        float passing_directness{0.5f};
        float tempo{0.5f};
    };

    fm::MatchSimulator simulator_;
    fm::MatchState current_state_;
    fm::MatchRecording recording_;
    fm::CheckpointManager checkpoint_mgr_;
    uint64_t initial_seed_{12345};
    PendingTacticalEdit pending_home_edit_{};
    PendingTacticalEdit pending_away_edit_{};
    double last_sim_time_ms_{0.0};

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
    void run_sandbox_demo();

    // Low-level buffer access for Godot visualization
    [[nodiscard]] PackedFloat32Array get_frame_data(int tick) const;
    [[nodiscard]] Dictionary get_match_metadata() const;
    [[nodiscard]] Dictionary get_tick_hud_state(int tick) const;
    [[nodiscard]] Array get_match_events() const;
    [[nodiscard]] int get_total_ticks() const;
    [[nodiscard]] int get_checkpoint_count() const;
    [[nodiscard]] double get_last_sim_time_ms() const;
    [[nodiscard]] Dictionary get_team_tactics(int team_id) const;

    // Tactical Mutators & Snapshot Rollback API
    void apply_tactical_change(int team_id, int formation_id, float defensive_line, float pressing);
    void apply_tactical_change_full(int team_id,
                                    int formation_id,
                                    float defensive_line,
                                    float pressing,
                                    float passing_directness,
                                    float tempo);
    void rollback_and_resimulate(int target_tick);
};

} // namespace godot
