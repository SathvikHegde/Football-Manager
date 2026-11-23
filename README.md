# Football Manager

## Prerequisites

- **C++20 Compiler:** MSVC (Visual Studio 2022 / 2026), GCC 11+, or Clang 16+
- **CMake:** Version 3.20 or newer
- **Ninja:** Build generator (recommended)
- **Python 3:** Version 3.4+ in system PATH (required by godot-cpp)
- **Git:** Required for submodule management
- **Godot Engine:** Version 4.7+ (to run the 2D Pitch Viewer frontend client)

---

## 1. Setup & Submodules

Clone the repository with submodules:

```bash
git clone --recursive <repository-url>
cd Football-Manager
```

If already cloned without submodules:

```bash
git submodule update --init --recursive
```

---

## 2. Build Instructions

### Command Line (CMake Presets)

```powershell
# Configure
cmake --preset windows-debug

# Build all targets (fm_core, fm_tests, fm_sim_cli, fm_gdextension)
cmake --build --preset windows-debug
```

*(For Release, replace `windows-debug` with `windows-release`)*

The compiled GDExtension library (`fm_gdextension.dll` / `.so` / `.dylib`) is automatically placed into `godot_client/bin/`.

### Visual Studio IDE

1. Open Visual Studio.
2. Select **File** > **Open** > **Folder...** and select the repository root.
3. Select preset **Windows x64 Debug** or **Windows x64 Release**.
4. Press **Ctrl + Shift + B** (or **Build** > **Build All**).

---

## 3. Running the Verification Test Suite

Run unit and physics tests via CTest:

```powershell
ctest --preset windows-debug
```

Or run the test executable directly:

```powershell
./build/debug/tests/fm_tests
```

---

## 4. Running the Standalone Simulation CLI

Execute the headless simulation CLI:

```powershell
./build/debug/cli_harness/fm_sim_cli
```

---

## 5. Running the 2D Pitch Viewer (Godot Client)

### Headless Verification

Verify that GDExtension loads and runs the 1,000-tick sandbox without GUI:

```powershell
godot --headless --path godot_client --quit-after 30
```

### Interactive 2D Pitch Viewer

Launch the 2D viewer:

```powershell
godot --path godot_client
```

### Viewer Playback Controls

| Control | Action |
| :--- | :--- |
| **`Space`** or **Play/Pause Button** | Toggle timeline playback |
| **`Left Arrow`** / **`Right Arrow`** or **⏮ / ⏭ Buttons** | Step back / forward by single frame |
| **`R`** or **↺ Restart Button** | Reset playback to frame 0 |
| **`1`, `2`, `3`, `4`** or **Speed Dropdown** | Set playback speed to 1x, 2x, 4x, 8x |
| **Scrub Slider (`HSlider`)** | Click or drag to jump to any tick (0..999) |
