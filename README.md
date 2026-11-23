# Football Manager

## Prerequisites

- **C++20 Compiler:** MSVC (Visual Studio 2022 / 2026), GCC 11+, or Clang 16+
- **CMake:** Version 3.20 or newer
- **Ninja:** Build generator (recommended)
- **Python 3:** Version 3.4+ in system PATH (required to generate GDExtension bindings)
- **Git:** Required for submodule management
- **Godot Engine:** Version 4.7+ (to run the frontend client)

---

## 1. Clone the Repository

Clone the repository along with the `godot-cpp` submodule:

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

### Option A: Visual Studio IDE

1. Open Visual Studio.
2. Select **File** > **Open** > **Folder...** and choose the `Football-Manager` repository root.
3. In the configuration dropdown, select **Windows x64 Debug** or **Windows x64 Release**.
4. Press **Ctrl + Shift + B** (or select **Build** > **Build All**) to build all targets.

### Option B: Command Line (CMake Presets)

Using the built-in CMake presets:

```powershell
# Configure
cmake --preset windows-debug

# Build all targets (fm_core, fm_tests, fm_sim_cli, fm_gdextension)
cmake --build --preset windows-debug
```

*(For Release, replace `windows-debug` with `windows-release`)*

### Option C: Manual CMake

```powershell
# Configure
cmake -B build -S . -G Ninja

# Build all targets
cmake --build build
```

The compiled GDExtension library (`fm_gdextension.dll` / `.so` / `.dylib`) is automatically placed into `godot_client/bin/`.

---

## 3. Running the Test Suite

Run unit tests via CTest:

```powershell
# Using Presets
ctest --preset windows-debug

# Or from custom build directory
ctest --test-dir build --output-on-failure
```

---

## 4. Running the Headless Simulation CLI

Execute the standalone simulation CLI:

```powershell
# When built with presets
.\build\debug\cli_harness\fm_sim_cli.exe

# When built manually
.\build\cli_harness\fm_sim_cli.exe
```

---

## 5. Running the Godot Client

Launch the project in Godot 4.7+:

```powershell
# Open project in Godot Editor
godot --editor --path godot_client

# Run the project directly
godot --path godot_client
```
