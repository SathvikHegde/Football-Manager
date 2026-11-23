# Football Manager - Build & Stage Progress

- **Current Status:** Stage 1 COMPLETE. Stage 2 READY. (Engine v0.1.0-stage1)
- **Godot Setup:** Godot 4.7.2 (`C:\Programs\Godot\Godot_v4.7.2-stable_win64_console.exe`).
- **godot-cpp:** `gdextension/godot-cpp` with `GODOTCPP_USE_STATIC_CPP=OFF` (`/MD`/`/MDd`).

### Working Commands
```powershell
# Build all targets (fm_core, fm_tests, fm_sim_cli, fm_gdextension)
cmd.exe /c "call `"C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat`" && cmake --preset windows-debug && cmake --build --preset windows-debug"

# Run unit tests (18/18 passed, 188,855 assertions) & CLI
ctest --preset windows-debug
.\build\debug\cli_harness\fm_sim_cli.exe

# Run Godot client verification (headless) / Launch 2D Pitch Viewer (interactive)
& "C:\Programs\Godot\Godot_v4.7.2-stable_win64_console.exe" --headless --path godot_client --quit-after 30
& "C:\Programs\Godot\Godot_v4.7.2-stable_win64_console.exe" --path godot_client
```

### Deviations from SPEC.md
1. **Testing Harness:** Catch2-compatible C++20 runner (`tests/test_framework.hpp`) for 100% offline builds.
2. **MSVC Debug Format:** Used `/Z7` embedded debug info to avoid background `mspdbsrv.exe` server locks.
