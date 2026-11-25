# Football Manager - Build & Stage Progress

- **Current Status:** Stage 3 COMPLETE. Stage 4 READY. (Engine v0.3.0-stage3)
- **Godot Setup:** Godot 4.7.2 (`C:\Programs\Godot\Godot_v4.7.2-stable_win64_console.exe`).
- **godot-cpp:** `gdextension/godot-cpp` with `GODOTCPP_USE_STATIC_CPP=OFF` (`/MD`/`/MDd`).

### Working Commands
```powershell
# Build all targets (fm_core, fm_tests, fm_sim_cli, fm_gdextension)
cmd.exe /c "call `"C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat`" && cmake --preset windows-debug && cmake --build --preset windows-debug"

# Run unit tests (30/30 passed, 35,624,560 assertions) & CLI (90m match, 108,000 ticks)
cmd.exe /c "call `"C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat`" && ctest --preset windows-debug --output-on-failure"
.\build\debug\tests\fm_tests.exe
.\build\debug\cli_harness\fm_sim_cli.exe 3735928559

# Run Godot client verification (headless) / Launch 2D Match Viewer (interactive)
& "C:\Programs\Godot\Godot_v4.7.2-stable_win64_console.exe" --headless --path godot_client --quit-after 30
& "C:\Programs\Godot\Godot_v4.7.2-stable_win64_console.exe" --path godot_client
```

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
2. **MSVC Debug Format:** Used `/Z7` embedded debug info to avoid background `mspdbsrv.exe` server locks.
