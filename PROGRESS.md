# Football Manager - Build & Stage Progress

- **Current Status:** Stage 4.5A COMPLETE. (3D Match Viewer Core; Engine v0.4.0-stage4)
- **Godot Setup:** Godot 4.7.2 (`C:\Programs\Godot\Godot_v4.7.2-stable_win64_console.exe`).
- **godot-cpp:** `gdextension/godot-cpp` with `GODOTCPP_USE_STATIC_CPP=OFF` (`/MD`/`/MDd`).

### Working Commands
```powershell
# Configure & Build all targets (fm_core, fm_tests, fm_sim_cli, fm_gdextension) in Release & Debug
cmd.exe /c "call `"C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat`" && cmake --preset windows-release -DPython3_EXECUTABLE=C:/Programs/Python/python.exe && cmake --build --preset windows-release"
cmd.exe /c "call `"C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat`" && cmake --preset windows-debug && cmake --build --preset windows-debug"

# Run automated verification suites (34/34 passed; Release runs full 200-match equal & +4 calibration DoD in ~16.3s)
cmd.exe /c "call `"C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat`" && ctest --preset windows-release --output-on-failure"
cmd.exe /c "call `"C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat`" && ctest --preset windows-debug --output-on-failure"

# Run Headless Multi-Threaded Calibration CLI (Stage 4 DoD: 200 matches in ~5.0-7.1s across 8 threads)
.\build\release\cli_harness\fm_sim_cli.exe --matches 200 --threads 8 --home-rating 12 --away-rating 12 --out build\release\report.csv --validate
.\build\release\cli_harness\fm_sim_cli.exe --matches 200 --threads 8 --home-rating 16 --away-rating 12 --out build\release\report_lead4.csv --validate
.\build\release\cli_harness\fm_sim_cli.exe 3735928559

# Run Godot client verification (headless) / Launch 3D & 2D Match Viewer (interactive)
& "C:\Programs\Godot\Godot_v4.7.2-stable_win64_console.exe" --headless --path godot_client --quit-after 30
& "C:\Programs\Godot\Godot_v4.7.2-stable_win64_console.exe" --path godot_client
```

### Stage 4.5A Summary
- **Shared Playback Controller & Runtime 2D/3D Toggle (`godot_client/`):** Extracted match simulation lifecycle, frame interpolation with `> 6.0 m` teleport-snap guard, speed selection (`1x`/`2x`/`4x`/`8x`/`16x`), timeline scrubbing/stepping, and tactical rollback coordination into `MatchPlaybackController` (`godot_client/scripts/match_playback_controller.gd`), driving both `ViewContainer2D` (`pitch_view.tscn` / `pitch_renderer.gd`) and `ViewContainer3D` (`pitch_3d.tscn` / `pitch_renderer_3d.gd`) in `main.tscn` & `main.gd`.
- **Procedural Pitch & Goal Net Shaders (`godot_client/shaders/`):** Implemented `pitch_surface.gdshader` on a `125 m × 84 m` `PlaneMesh` (`5.25 m` mowed grass stripes, apron turf, procedural grain, and `0.12 m` anti-aliased FIFA/`fm_core` markings) and translucent `goal_net.gdshader` (`cull_disabled` diamond net grid).
- **3D Pitch, Goals, Ball & Placeholder Players (`godot_client/scenes/`, `godot_client/scripts/`):** Built `pitch_3d.tscn`, `player_3d.tscn`, and `PitchRenderer3D` (`pitch_renderer_3d.gd`) using 1:1 coordinate parity `(x_sim, z_sim, -y_sim)` and `rotation.y = facing_rad - PI * 0.5`, with `HomeGoal`/`AwayGoal` (`7.32 m × 2.44 m × 2.0 m`), Telstar-paneled rolling `BallMesh` (`r = 0.11 m`), elevation-scaled `BallShadow`, `BallCarrierRing`, and 22 `ModelPivot`-wrapped `Player3D` capsules with facing cones and jersey numbers.
- **3D Camera Rig & HUD Controls (`godot_client/scripts/camera_rig_3d.gd`):** Added `CameraRig3D` supporting `Broadcast (TV)` (`F1`), `Tactical 3D` (`F2`), `Behind Goal` (`F3`, `±12 m` midfield hysteresis), `Free Orbit` (`F4`, WASD pan / RMB orbit / scroll zoom), and `2D Classic` (`F5`) plus `V` view cycling and `TopBar/ViewButtons`, with exponential damping and instant scrub/rollback snapping.
- **Verification:** Zero edits to `core/` or `gdextension/`; `ctest --preset windows-release` (`14.82s`) and `ctest --preset windows-debug` (`31.78s`) passed `100%`; Godot headless smoke test (`--headless --path godot_client --quit-after 30`) and headless parity/rollback check exited with code `0` and zero script or shader errors.

### Stage 4 Summary
- **Headless Telemetry & Team Rating Contracts (`core/`):** Added `MatchStats` / `TeamMatchStats` and `MatchSimulator::simulate_headless(...)` in `fm/engine/match_stats.hpp` & `fm/engine/match_simulator.hpp` for zero-snapshot-allocation 90-minute match telemetry, plus `make_role_attributes` and `apply_team_rating` in `fm/model/team.hpp` (`core/src/model/team.cpp`).
- **Multi-Threaded Calibration CLI (`cli_harness/`):** Implemented `BatchConfig`, `BatchAggregate`, `run_headless_batch` (`std::thread` + atomic work queue + SplitMix64 seed derivation), `write_batch_csv`, `print_batch_summary`, and `--validate` band checking in `cli_harness/batch_runner.hpp`, `cli_harness/batch_runner.cpp`, and `cli_harness/main.cpp` (`--matches`, `--threads`, `--home-rating`, `--away-rating`, `--home-formation`, `--away-formation`, `--mix-formations`, `--seed`, `--out`, `--validate`, plus legacy single-seed positional mode).
- **Utility AI, Execution & Goalkeeper Calibration (`core/src/ai/`, `core/src/engine/`):** Added `pass_target_slot` receiver tracking, drag-aware goal-plane shot prediction (`predict_goal_plane_arrival`), defender shot blocking, restart lane clearance checks, passing lane/loft/dispersion tuning, and goalkeeper catch/parry/reach calibration:
  - Equal Teams (`12 vs 12`, 200 matches, 8 threads, ~5.38s): `2.77` goals/match (`2.4-3.0`), `23.74` shots/match (`20-28`), `35.5%` SoT (`30-42%`), `83.8%` pass completion (`78-86%`), `50.1%` possession (`48-52%`).
  - `+4` Attribute Lead (`16 vs 12`, 200 matches, 8 threads, ~5.00-7.08s): `3.52` goals/match (`3.2-4.1`), `87.0%` win rate (`>65%`), `27.45` shots/match (`24-32`), `42.4%` SoT (`35-48%`), `83.1%` pass completion (`82-89%`), `59.0%` possession (`58-66%`).
- **Automated Verification (`tests/`):** Added `tests/test_calibration.cpp` validating role specialization/monotonicity, bit-identical equivalence between `simulate_headless` and `simulate_match`, thread-count-independent batch determinism (1 vs 8 threads), and full Stage 4 200-match calibration & `<30s` throughput assertions.

### Stage 3 Summary
- **Checkpointed Pre-Simulation & Rollback Engine (`core/`):** Added `MatchSimulator::simulate_match(..., CheckpointManager&)`, `resume_simulation(...)`, `rollback_to_tick(...)`, and `rollback_and_resimulate(...)`, capturing 90 full `MatchState` snapshots (every `1,200` ticks) plus `TacticalHistoryEntry` logs for deterministic mid-checkpoint and multi-edit rollbacks.
- **Tactical Mutation & Metadata (`core/`):** Added `apply_tactical_change`, `formation_name`, and `sanitize_formation` in `fm/model/tactics.hpp` updating formation anchors and player roles cleanly, plus per-frame tactical metadata in `FrameMeta` and `TacticalChange` truncation semantics in `MatchRecording::truncate`.
- **GDExtension Rollback Bridge (`gdextension/`):** Updated `MatchEngineBridge` with staged tactical mutations (`apply_tactical_change`, `apply_tactical_change_full`), `rollback_and_resimulate(target_tick)` delegation to `fm_core`, `get_checkpoint_count()`, `get_last_sim_time_ms()`, `get_team_tactics(team_id)`, and `TacticalChange` event formatting.
- **Timeline Scrubber & Tactical Panel (`godot_client/`):** Added `TacticalPanel` (`TeamOption`, `FormationOption`, `LineSlider`, `PressSlider`, `Apply & Resim` button) and quick-jump timeline buttons (`0'`, `HT`, `60'`, `75'`) in `main.tscn` / `main.gd`, plus tactical badge and `TacticalChange` feed formatting in `match_hud.gd`.
- **Automated Verification (`tests/`):** Added Stage 3 DoD tests in `tests/test_determinism.cpp` (full 108,000-frame bit-identical `0xDEADBEEF` validation) and `tests/test_rollback.cpp` (90-checkpoint memory/interval check, minute-60 identical rollback byte-equivalence, minute-60 low-block-to-high-press divergence, and mid-checkpoint multi-edit rollback).

### Stage 2 Summary
- **Layer 1 (Rules & Clock):** Half-Time (`54000` ticks) with second-half attack polarity inversion, Full-Time (`108000` ticks), Goal scoring + net containment, Throw-In, Corner, GoalKick, and set-piece exclusion/restart execution.
- **Layer 2 (Team Phase & Anchors):** All 5 formations (`4-4-2`, `4-3-3`, `4-2-3-1`, `3-5-2`, `5-3-2`), `TeamPhase` transition tracking (`InPossession`, `OutOfPossession`, `AttackingTransition`, `DefensiveTransition`), dynamic ball-shifted anchors with out-of-possession 15% width compression and winger extension.
- **Layer 3 (Utility AI & Goalkeeper AI):** Staggered 4-tick cadence with possession/loose-ball interrupts, On-Ball (`Shoot`, `Pass`, 8-ray `Dribble`, `Shield`) & Off-Ball (`Intercept`, `Press`, `Support`, `Forward Run`, `Cover Anchor`) scoring with `decisions`-scaled Gaussian noise, plus GK angle-bisector positioning and dive-reach save evaluation.
- **Layer 4 (Execution & Kinematics):** Attribute-driven Gaussian directional/power execution variance, contested tackling, loose-ball claiming, stamina drain, and deadlock guards.
- **Godot MatchHUD & Bridge:** Added `get_tick_hud_state(tick)` and `get_match_events()` to `MatchEngineBridge`, wired `MatchHUD` (`ScoreboardBar`, `MM:SS` clock, play state badge, live stats, and timestamped event log) in `main.tscn` / `main.gd`.

### Deviations from SPEC.md
1. **Testing Harness:** Catch2-compatible C++20 runner (`tests/test_framework.hpp`) for 100% offline builds.
2. **MSVC Debug Format:** Used `/Z7` embedded debug info across Debug & Release presets to avoid background `mspdbsrv.exe` server locks.
