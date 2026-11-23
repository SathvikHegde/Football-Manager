# Football Manager - Build & Stage Progress

- **Current Status:** Stage 0 COMPLETE. Stage 1 READY.
- **Godot Setup:** Godot 4.7.2 installed (`C:\Programs\Godot\Godot_v4.7.2-stable_win64_console.exe`).
- **godot-cpp Setup:** Submodule at `gdextension/godot-cpp` (master/4.7 API) using `GODOTCPP_USE_STATIC_CPP=OFF` (`/MD`/`/MDd`).

### Working Commands
```powershell
# Build all targets (fm_core, fm_tests, fm_sim_cli, fm_gdextension)
cmake --preset windows-debug && cmake --build --preset windows-debug

# Run unit tests (11/11 passed, 5,556 assertions)
ctest --preset windows-debug

# Run Godot client verification (prints ping() hello-world)
& "C:\Programs\Godot\Godot_v4.7.2-stable_win64_console.exe" --headless --path godot_client --quit-after 20
```

### Deviations from SPEC.md
1. **Testing Harness:** Included Catch2-compatible C++20 runner (`tests/test_framework.hpp`) to allow 100% offline builds without network FetchContent.
2. **MSVC Debug Format:** Used `/Z7` embedded debug format to prevent MSVC `mspdbsrv.exe` PDB server locks.

