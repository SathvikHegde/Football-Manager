# Football Manager - Build & Stage Progress

- **Current Status:** Stage 2 COMPLETE. Stage 3 READY. (Engine v0.2.0-stage2)
- **Godot Setup:** Godot 4.7.2 (`C:\Programs\Godot\Godot_v4.7.2-stable_win64_console.exe`).
- **godot-cpp:** `gdextension/godot-cpp` with `GODOTCPP_USE_STATIC_CPP=OFF` (`/MD`/`/MDd`).

### Working Commands
```powershell
# Build all targets (fm_core, fm_tests, fm_sim_cli, fm_gdextension)
cmd.exe /c "call `"C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat`" && cmake --preset windows-debug && cmake --build --preset windows-debug"

# Run unit tests (26/26 passed, 25,495,913 assertions) & CLI (90m match, 108,000 ticks)
cmd.exe /c "call `"C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat`" && ctest --preset windows-debug --output-on-failure"
.\build\debug\tests\fm_tests.exe
.\build\debug\cli_harness\fm_sim_cli.exe 12345

# Run Godot client verification (headless) / Launch 2D Match Viewer (interactive)
& "C:\Programs\Godot\Godot_v4.7.2-stable_win64_console.exe" --headless --path godot_client --quit-after 30
& "C:\Programs\Godot\Godot_v4.7.2-stable_win64_console.exe" --path godot_client
```

### Stage 2 Summary
- **Layer 1 (Rules & Clock):** Half-Time (`54000` ticks) with second-half attack polarity inversion, Full-Time (`108000` ticks), Goal scoring + net containment, Throw-In, Corner, GoalKick, and set-piece exclusion/restart execution.
- **Layer 2 (Team Phase & Anchors):** All 5 formations (`4-4-2`, `4-3-3`, `4-2-3-1`, `3-5-2`, `5-3-2`), `TeamPhase` transition tracking (`InPossession`, `OutOfPossession`, `AttackingTransition`, `DefensiveTransition`), dynamic ball-shifted anchors with out-of-possession 15% width compression and winger extension.
- **Layer 3 (Utility AI & Goalkeeper AI):** Staggered 4-tick cadence with possession/loose-ball interrupts, On-Ball (`Shoot`, `Pass`, 8-ray `Dribble`, `Shield`) & Off-Ball (`Intercept`, `Press`, `Support`, `Forward Run`, `Cover Anchor`) scoring with `decisions`-scaled Gaussian noise, plus GK angle-bisector positioning and dive-reach save evaluation.
- **Layer 4 (Execution & Kinematics):** Attribute-driven Gaussian directional/power execution variance, contested tackling, loose-ball claiming, stamina drain, and deadlock guards.
- **Godot MatchHUD & Bridge:** Added `get_tick_hud_state(tick)` and `get_match_events()` to `MatchEngineBridge`, wired `MatchHUD` (`ScoreboardBar`, `MM:SS` clock, play state badge, live stats, and timestamped event log) in `main.tscn` / `main.gd`.

### Deviations from SPEC.md
1. **Testing Harness:** Catch2-compatible C++20 runner (`tests/test_framework.hpp`) for 100% offline builds.
2. **MSVC Debug Format:** Used `/Z7` embedded debug info to avoid background `mspdbsrv.exe` server locks.
