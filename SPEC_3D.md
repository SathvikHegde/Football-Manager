# Football Manager Clone: 3D Match Viewer Specification (`SPEC_3D.md`)

## 1. Purpose, Baseline & Strict Engineering Rules

This document specifies **Stages 4.5A through 4.5C**: adding a full 3D Match Viewer to the existing Football Manager clone. It is a direct extension of `SPEC.md`.

### Current Project Baseline (Do Not Assume Anything Beyond This)
Only **Stages 0 through 4** from `SPEC.md` are implemented right now:
* `fm_core` (C++20 static library): Deterministic $20\text{ Hz}$ ($DT = 0.05\text{ s}$) match simulation over a $105\text{ m} \times 68\text{ m}$ pitch, 10 player attributes, 22 agents (`0..10` Home, `11..21` Away; slots `0` and `11` are goalkeepers), Utility AI, GK bisector/dive logic, basic match rules (`Kickoff`, `OpenPlay`, `ThrowIn`, `GoalKick`, `Corner`, `GoalScored`, `HalfTime`, `FullTime`), `MatchRecording`, and 1-minute `CheckpointManager` rollback.
* `fm_gdextension`: `MatchEngineBridge` streaming 93-float `PackedFloat32Array` frames per tick, match metadata, total ticks, tactical mutators, and `rollback_and_resimulate(target_tick)`.
* `godot_client`: Working 2D top-down pitch viewer (`pitch_renderer.gd`), playback controls, and `match_hud.gd`.
* `cli_harness` & `tests`: Calibrated `fm_sim_cli` batch runner and passing determinism/physics/rollback unit tests.

**Do NOT assume or implement** offsides, fouls, cards, substitutions, dual formations, player roles, SQLite databases, or extra attributes in this milestone.

### Strict Rules for the Coding LLM
1. **Zero Simulation Changes in `core/`:** Do **not** alter physics, Utility AI, RNG consumption, `MatchState`, or tick stepping inside `core/`. All existing `ctest` suites and Stage 4 `fm_sim_cli` outputs must remain **byte-identical**.
2. **Keep 2D Classic Fully Functional:** The existing 2D pitch renderer (`pitch_renderer.gd`) must stay intact and toggleable at runtime alongside the new 3D view (`2D Classic` $\leftrightarrow$ `3D Broadcast / Tactical`).
3. **No Per-Entity Per-Tick FFI Calls:** The 3D viewer must consume the exact same 93-float `PackedFloat32Array` frame layout defined in `SPEC.md` Section 6.1.
4. **Minimal Read-Only Bridge Extension Only:** The only permitted C++ change is adding a single read-only bulk event getter (`get_events_packed()`) to `gdextension/` (if not already present) so Godot can drive look-ahead animations without per-tick FFI calls.
5. **Respect Local Build Configuration:** Read `PROGRESS.md` and `CMakePresets.json` for the exact CMake presets, compiler toolchains, and Godot console executable paths on this machine.
6. **Headless & Asset-Safe Fallbacks:** The project must run cleanly in Godot headless smoke tests and work even before external `.glb` character or stadium meshes are dropped into `godot_client/assets/`.
7. **Stage-Gated Execution:** Implement strictly one sub-stage per LLM session (`4.5A`, then `4.5B`, then `4.5C`).

---

## 2. Coordinate Mapping & Spatial Parity (1:1 Metric Alignment)

`fm_core` uses a **right-handed Z-up** coordinate system in meters. Godot 4 3D uses a **right-handed Y-up** coordinate system in meters.

### 2.1 Position Transform
Map simulation coordinates $(x_{\text{sim}}, y_{\text{sim}}, z_{\text{sim}})$ to Godot 3D world space $(x_{\text{3d}}, y_{\text{3d}}, z_{\text{3d}})$ using:

$$(x_{\text{3d}}, y_{\text{3d}}, z_{\text{3d}}) = (x_{\text{sim}}, \; z_{\text{sim}}, \; -y_{\text{sim}})$$

* **Pitch Length ($X$):** $x_{\text{3d}} \in [-52.5, +52.5]\text{ m}$. Home goal line at $x_{\text{3d}} = -52.5\text{ m}$; Away goal line at $x_{\text{3d}} = +52.5\text{ m}$.
* **Elevation ($Y$):** $y_{\text{3d}} = 0.0\text{ m}$ is the turf surface; positive $y_{\text{3d}}$ is upward.
* **Pitch Width ($Z$):** $z_{\text{3d}} \in [-34.0, +34.0]\text{ m}$.
* **Ball Vertical Offset:** Because `ball.position.z == 0.0` in `fm_core` represents the ball resting on the turf, position the visual ball mesh (radius $r_{\text{ball}} = 0.11\text{ m}$) at:

$$\vec{p}_{\text{ball, 3d}} = (x_{\text{sim}}, \; z_{\text{sim}} + 0.11, \; -y_{\text{sim}})$$

* **Handedness Check:** $\hat{X} \times (-\hat{Z}) = +\hat{Y}$, so this transform is a proper rigid rotation ($\det = +1$) with zero mirroring distortion.

### 2.2 Facing Angle Transform
In `fm_core`, `facing_rad` $= \theta$ has $\theta = 0$ pointing along $+X$ and increases counter-clockwise toward $+Y$. In Godot 3D, that unit heading vector is:

$$\vec{d}_{\text{3d}}(\theta) = (\cos\theta, \; 0.0, \; -\sin\theta)$$

For a Godot `Node3D` whose canonical forward axis is `-Z`, the corresponding Y-axis Euler rotation is:

$$\text{rotation.y} = \theta - \frac{\pi}{2}$$

* Verify $\theta = 0 \implies \text{rotation.y} = -\frac{\pi}{2} \implies -Z \text{ rotates to } +X$.
* Verify $\theta = \frac{\pi}{2} \implies \text{rotation.y} = 0 \implies -Z \text{ stays } -Z$ (which maps from $+Y_{\text{sim}}$).
* **Model Forward Offset:** External glTF/Mixamo rigs commonly face `+Z` instead of `-Z`. Every `PlayerVisual3D` must wrap its mesh inside a `ModelPivot` child node with an exported `@export var model_yaw_offset_rad: float = PI` (or `0.0` for `-Z`-facing assets) so the simulation heading math never has to change per asset.

### 2.3 Frame Interpolation
Playback runs at display FPS (60+ FPS) while `fm_core` ticks at $20\text{ Hz}$ ($DT = 0.05\text{ s}$). Given continuous playback tick $t_f \in [0, N-1]$:
* Let $k = \lfloor t_f \rfloor$, $k_1 = \min(k + 1, N - 1)$, and $\alpha = t_f - k \in [0, 1)$.
* Fetch `frame_a = bridge.get_frame_data(k)` and `frame_b = bridge.get_frame_data(k_1)`.
* Linearly interpolate all 22 player positions and the 3D ball position via `lerp(a, b, alpha)`.
* Interpolate player facing angles via shortest-arc `lerp_angle(theta_a, theta_b, alpha)`.
* **Teleport Guard:** If ball displacement between tick $k$ and $k_1$ exceeds $6.0\text{ m}$ in a single $0.05\text{ s}$ tick (e.g., kickoff or set-piece reset snap), snap ball position to `frame_b` instead of lerping across the pitch.

---

## 3. Repository Additions & Scene Architecture

Add the following files without breaking the existing `SPEC.md` directory tree:

```text
football_manager/
├── gdextension/
│   ├── include/match_engine_bridge.hpp      # Add read-only get_events_packed()
│   └── src/match_engine_bridge.cpp
└── godot_client/
    ├── assets/                              # User drop-in folder (gitignored for large packs if desired)
    │   ├── characters/                      # Optional: footballer.glb + animations
    │   └── stadium/                         # Optional: stadium.glb
    ├── shaders/
    │   ├── pitch_surface.gdshader           # Procedural 105m x 68m markings + grass stripes
    │   ├── goal_net.gdshader                # Translucent diamond/grid goal net
    │   ├── player_kit.gdshader              # Per-instance uniform team/GK/skin colors
    │   └── crowd_spectator.gdshader         # MultiMesh per-instance colors + goal cheer bobble
    ├── scenes/
    │   ├── main.tscn                        # Updated: hosts MatchPlaybackController, 2D view, 3D view, HUD
    │   ├── pitch_view.tscn                  # Existing 2D Classic viewer (kept intact)
    │   ├── pitch_3d.tscn                    # New 3D pitch, goals, ball, players, cameras, stadium
    │   └── player_3d.tscn                   # Reusable 3D player node (rig + fallback capsule + nameplate)
    └── scripts/
        ├── match_playback_controller.gd     # Single source of truth for tick, speed, scrubbing, 2D/3D toggle
        ├── pitch_renderer.gd                # Existing 2D renderer (receives interpolated frame or ticks)
        ├── pitch_renderer_3d.gd             # 3D world updater (ball roll, shadow, 22 players, stadium effects)
        ├── player_visual_3d.gd              # Locomotion speed calc, AnimationTree driver, kit uniforms
        ├── animation_director.gd            # Pre-computed event look-ahead queue + scrub/rollback reset
        ├── camera_rig_3d.gd                 # Broadcast, Tactical 3D, Behind Goal, Free Orbit cameras
        └── stadium_builder_3d.gd            # Procedural stands + MultiMeshInstance3D crowd (or .glb loader)
```

### Runtime Scene Tree (`main.tscn`)
```text
Main (Node)
├── MatchEngineBridge (GDExtension Node)
├── MatchPlaybackController (Node)
├── ViewContainer2D (CanvasLayer or SubViewportContainer / Node2D)
│   └── PitchView2D (existing pitch_view.tscn)
├── ViewContainer3D (Node3D, toggled via visible + active Camera3D)
│   └── Pitch3D (pitch_3d.tscn)
│       ├── WorldEnvironment
│       ├── DirectionalLight3D (Sun/Floodlight)
│       ├── PitchSurface (MeshInstance3D, 125m x 84m PlaneMesh + pitch_surface.gdshader)
│       ├── Goals (Node3D: HomeGoal at X=-52.5, AwayGoal at X=+52.5)
│       ├── BallRoot (Node3D)
│       │   ├── BallMesh (SphereMesh r=0.11m)
│       │   └── BallShadow (Decal or flat MeshInstance3D quad at Y=0.01m)
│       ├── PlayersContainer (Node3D: 22x Player3D instances, slots 0..21)
│       ├── BallCarrierRing (MeshInstance3D torus/ring on turf under owner_slot)
│       ├── CameraRig3D (Node3D + Camera3D)
│       ├── AnimationDirector (Node)
│       └── StadiumRoot (Node3D: procedural stands/MultiMesh crowd or optional stadium.glb)
└── MatchHUD (CanvasLayer: existing controls + View/Camera selector buttons)
```

---

## 4. Minimal GDExtension Contract Extension (Event Stream)

To support pre-computed animation look-ahead in Stage 4.5B with zero per-tick FFI overhead, expose the recorded `MatchEvent` array as a single flat `PackedFloat32Array` on `MatchEngineBridge`:

```cpp
// gdextension/include/match_engine_bridge.hpp (addition only)
[[nodiscard]] PackedFloat32Array get_events_packed() const;
```

### Flat Event Wire Layout (`FLOATS_PER_EVENT = 6`)
Each event $e$ occupies 6 consecutive floats in chronological order (sorted by `tick`):

```text
[e*6 + 0]: tick            (float, exact integer tick 0..108000)
[e*6 + 1]: event_type      (float, cast from fm::EventType uint8_t enum value)
[e*6 + 2]: primary_slot    (float, -1.0 or 0..21)
[e*6 + 3]: secondary_slot  (float, -1.0 or 0..21)
[e*6 + 4]: location.x      (float, meters in fm_core space)
[e*6 + 5]: location.y      (float, meters in fm_core space)
```

* Godot calls `get_events_packed()` **only once** after `simulate_full_match()` and once after any `rollback_and_resimulate(target_tick)`.
* `EventType` enum integer values must match `core/include/fm/model/match_event.hpp` exactly (`Kickoff=0`, `Pass=1`, ..., `TacticalChange=17`). Define matching constants in GDScript.

---

## 5. Technical Specifications for 3D Subsystems

### 5.1 Procedural Pitch Surface Shader (`pitch_surface.gdshader`)
Render the entire pitch and surrounding grass apron on a single `PlaneMesh` of size $125.0\text{ m} \times 84.0\text{ m}$ centered at `(0, 0, 0)` (providing a $10.0\text{ m}$ apron behind each goal line and $8.0\text{ m}$ outside each touchline).

Pass local vertex position `varying vec3 v_pos;` from the vertex shader (`v_pos = VERTEX;`) and evaluate markings in exact meters $(x, z) = (\text{v\_pos.x}, \text{v\_pos.z})$:
* **Mowed Grass Stripes:** Alternate two realistic turf albedo greens every $5.25\text{ m}$ along $X$ (20 stripes across $105\text{ m}$), plus subtle high-frequency procedural noise for grass grain and higher roughness (`0.85`).
* **Anti-Aliased White Lines:** Line thickness $w = 0.12\text{ m}$ (half-width $h = 0.06\text{ m}$). Use `fwidth()` screen-space derivatives (`float aa = max(fwidth(dist), 0.015);`) with `smoothstep(h + aa, h - aa, dist)` so lines stay crisp without aliasing from elevated broadcast cameras.
* **Exact FIFA / `fm_core` Markings:**
  * Boundary rectangle: $|x| = 52.5$ for $|z| \le 34.0$, and $|z| = 34.0$ for $|x| \le 52.5$.
  * Halfway line: $x = 0.0$ for $|z| \le 34.0$, plus center spot ($r = 0.22\text{ m}$) and center circle ($r = 9.15\text{ m}$).
  * Penalty areas (both ends): $|x| \in [36.0, 52.5]$ and $|z| \le 20.16$.
  * Goal areas / 6-yard boxes (both ends): $|x| \in [47.0, 52.5]$ and $|z| \le 9.16$.
  * Penalty spots: $(|x| - 41.5)^2 + z^2 \le 0.22^2$.
  * Penalty arcs: $|\sqrt{(|x| - 41.5)^2 + z^2} - 9.15| \le h$ constrained to $|x| < 36.0$.
  * Corner arcs: $|\sqrt{(|x| - 52.5)^2 + (|z| - 34.0)^2} - 1.0| \le h$ constrained to $|x| \le 52.5$ and $|z| \le 34.0$.

### 5.2 Goals & Ball Visuals
* **Goal Frames:** Build two reusable `Goal3D` assemblies at $x = -52.5\text{ m}$ (opening toward $+X$) and $x = +52.5\text{ m}$ (opening toward $-X$):
  * Posts: `CylinderMesh` (radius $0.06\text{ m}$, height $2.44\text{ m}$) at $z = \pm 3.66\text{ m}$, $y = 1.22\text{ m}$.
  * Crossbar: `CylinderMesh` (radius $0.06\text{ m}$, length $7.32\text{ m}$) at $y = 2.44\text{ m}$.
  * Net depth: $2.0\text{ m}$ behind goal line, using a back-stanchion frame and translucent `goal_net.gdshader` grid (white lines, alpha $\approx 0.35$, `cull_disabled`).
* **Ball Mesh & Rolling Kinematics:**
  * `SphereMesh` radius $r = 0.11\text{ m}$ with high-contrast panel texture/shader so roll is visible.
  * Each rendered frame, compute horizontal displacement $\Delta \vec{p} = (x_t - x_{t-1}, 0, z_t - z_{t-1})$. If $\|\Delta \vec{p}\| > 0.0005\text{ m}$ and not teleported, rotate `BallMesh` around axis $\vec{u} = \hat{Y} \times \frac{\Delta \vec{p}}{\|\Delta \vec{p}\|}$ by angle $\Delta \phi = \frac{\|\Delta \vec{p}\|}{0.11}$ radians in global space.
* **Elevated Ball Ground Shadow:** Place a dark, soft radial-gradient quad at $(x_{\text{3d}}, 0.015, z_{\text{3d}})$. Scale its radius slightly larger and fade its opacity smoothly as $y_{\text{ball}}$ rises from $0\text{ m}$ to $12\text{ m}$ so the user can always read ball height instantly.

### 5.3 Camera Rig (`camera_rig_3d.gd`)
Provide 4 3D camera modes plus instant toggle to `CLASSIC_2D` (bound to hotkeys `V` to cycle or `F1`–`F5` direct select, plus HUD buttons):

| Mode | Hotkey | Position / Behavior | Purpose |
| :--- | :--- | :--- | :--- |
| **Broadcast (TV)** | `F1` | Elevated sideline at $z \approx +38\text{ m}$, $y \approx 22\text{ m}$, tracks $x \in [-32, +32]\text{ m}$. Target is weighted focus: $0.75 \cdot \vec{p}_{\text{ball}} + 0.25 \cdot \vec{p}_{\text{action\_centroid}}$. Dynamic FOV narrows from $38^\circ$ at midfield to $30^\circ$ inside penalty boxes. | Default cinematic FM 3D experience |
| **Tactical 3D** | `F2` | High wide sideline at $z \approx +48\text{ m}$, $y \approx 38\text{ m}$, FOV $42^\circ$, gentle horizontal follow ($x \in [-18, +18]\text{ m}$). | Reading defensive lines, spacing & passing triangles |
| **Behind Goal** | `F3` | Low end-line camera behind active attacking goal ($x = \pm 60\text{ m}$, $y = 6.5\text{ m}$, $z = 0$), switches ends smoothly when ball crosses midfield by $> 12\text{ m}$ (with hysteresis). | Shooting angles, GK dives & replays |
| **Free Orbit** | `F4` | WASD pan, right-mouse-drag yaw/pitch orbit, scroll-wheel zoom. | Inspecting physics, animations & stadium |
| **2D Classic** | `F5` | Hides `ViewContainer3D`, shows `ViewContainer2D` (existing `pitch_view.tscn`). | Classic FM tactical overview & parity check |

* **Smoothing:** Use framerate-independent exponential damping `1.0 - exp(-k * delta)` ($k \approx 4.5$ for position, $k \approx 6.0$ for look target).
* **Scrubbing Snap:** When the user drags the timeline slider or triggers rollback, snap the camera immediately to the new focus point without a sluggish pan across the field.

### 5.4 3D Player Rig, Locomotion & Kit Shader (`player_visual_3d.gd`)
* **Dual Visual Mode (Asset-Safe):**
  * **Fallback Mode (Stage 4.5A & missing asset):** Clean articulated placeholder (`CapsuleMesh` body $1.78\text{ m}$ tall, `SphereMesh` head, directional visor/cone indicating facing direction, team-colored torso/shorts, floating jersey number label).
  * **Skinned Character Mode (Stage 4.5B):** Automatically loads `res://assets/characters/footballer.glb` (if present) containing a standard humanoid `Skeleton3D` and `AnimationPlayer` with animations mapped via an `AnimationTree`.
* **Expected Animation Clip Names (Standardized Contract):**
  * Locomotion: `Idle`, `Walk`, `Jog`, `Sprint`
  * One-Shot Actions: `Pass`, `Shoot`, `Tackle`, `GK_Idle`, `GK_Dive_Left`, `GK_Dive_Right`, `Celebrate`
  * If a specific clip is missing from the user's imported `.glb`, log a single startup warning and safely fall back to the nearest locomotion clip instead of crashing.
* **Velocity-Driven Locomotion (`AnimationTree`):**
  * Compute instantaneous ground speed $v = \frac{\|\vec{p}_{k+1} - \vec{p}_k\|}{0.05}\text{ m/s}$ in Godot (smoothed with low-pass filter to prevent jitter).
  * Feed $v$ into a `BlendSpace1D` (`0.0` Idle, `1.8` Walk, `4.2` Jog, `7.5` Sprint).
  * Modulate locomotion `TimeScale` by `clamp(v / ref_speed, 0.7, 1.4) * playback_speed_multiplier` so stride cadence matches ground speed and scales with $1\times / 2\times / 4\times / 8\times$ match playback.
* **Per-Instance Kit Shader (`player_kit.gdshader`):**
  * Use Godot 4 `instance uniform vec4 kit_primary`, `instance uniform vec4 kit_secondary`, `instance uniform vec4 shorts_color`, and `instance uniform vec4 skin_tone` driven by a UV/vertex-color mask so all 22 players share one material.
  * Distinct visual kits:
    * Home Outfield (`1..10`): Red jersey / White shorts (or configurable team colors).
    * Home GK (`0`): Neon Yellow / Black.
    * Away Outfield (`12..21`): Royal Blue jersey / White shorts.
    * Away GK (`11`): Bright Green / Black.

### 5.5 Pre-Computed Event Look-Ahead Director (`animation_director.gd`)
Because the match is pre-computed in `MatchRecording`, firing a kick animation *at* the event tick makes the ball leave the foot before the leg swings. `AnimationDirector` fixes this by scanning ahead in the cached event array:

* **Lead-Time Table (ticks before event tick $T$, where $1\text{ tick} = 0.05\text{ s}$):**
  * `EventType::Pass` $\to$ trigger `Pass` on `primary_slot` at $T - 4$ ($0.20\text{ s}$ early).
  * `EventType::Shot` $\to$ trigger `Shoot` on `primary_slot` at $T - 5$ ($0.25\text{ s}$ early).
  * `EventType::TackleWon` $\to$ trigger `Tackle` on `primary_slot` at $T - 4$ ($0.20\text{ s}$ early).
  * `EventType::Save` $\to$ trigger `GK_Dive_Left` or `GK_Dive_Right` on `primary_slot` at $T - 5$ ($0.25\text{ s}$ early). Determine dive side from the sign of $(\vec{p}_{\text{ball}}(T) - \vec{p}_{\text{gk}}(T)) \cdot \vec{r}_{\text{gk}}(T)$, where $\vec{r}_{\text{gk}}$ is the goalkeeper's right vector.
  * `EventType::Goal` $\to$ trigger `Celebrate` on `primary_slot` (and nearby teammates within $15\text{ m}$) at $T + 2$, and trigger a 3-second stadium crowd cheer pulse.
* **High-Speed & Scrubbing Hygiene:**
  * At playback speeds $> 4\times$, skip non-essential one-shots (`Pass`, `Tackle`) to avoid manic animation strobing.
  * Maintain an advancing `next_event_index` cursor. If `abs(current_tick - last_processed_tick) > 10` (user scrubbed slider, pressed Restart, or executed rollback), clear all active one-shots and binary-search `next_event_index` to the new `current_tick`.

### 5.6 Stadium, Lighting & MultiMesh Crowd (`stadium_builder_3d.gd`)
* **Optional Drop-In Override:** If `res://assets/stadium/stadium.glb` exists, instantiate it inside `StadiumRoot` (centered at `(0, 0, 0)`). Otherwise, procedurally generate a clean 4-stand stadium bowl:
  * 4 tiered concrete/roof stands positioned outside the $125\text{ m} \times 84\text{ m}$ pitch apron, plus pitch-side advertising boards ($0.9\text{ m}$ tall, $4\text{ m}$ behind touchlines/goal lines) and 4 corner floodlight towers.
* **Single-Draw-Call Crowd (`MultiMeshInstance3D`):**
  * Populate 2,000–4,000 low-poly seated spectators across the stand tiers inside a single `MultiMeshInstance3D` with `use_colors = true`.
  * Assign per-instance colors biased 75% toward Home kit palette (Home ends/main stand) and 25% Away palette (Away section) plus neutral jackets.
  * Attach `crowd_spectator.gdshader` with a global shader uniform `crowd_excitement` ($0.0 \to 1.0$) that adds subtle vertical bobbing/cheering when a `ShotOnTarget`, `Save`, or `Goal` occurs.
* **Lighting & Visual Clarity:**
  * `DirectionalLight3D` angled cleanly for soft pitch shadows (PSSM 4-split, `directional_shadow_max_distance = 160.0`), `WorldEnvironment` with `ProceduralSkyMaterial` and ACES Filmic tonemapping.
  * Subtle glowing ring (`BallCarrierRing`) on the turf under `owner_slot` (hidden when `owner_slot == -1`) plus optional floating 3D `Label3D` showing active ball carrier slot/role.

---

## 6. Phased Implementation Plan (Stages 4.5A – 4.5C)

### Stage 4.5A: 3D Core Viewer, Procedural Pitch, Cameras & 2D/3D Toggle
**Scope:** Zero C++ changes and zero external 3D asset dependencies. Prove 1:1 coordinate parity and smooth 60 FPS 3D playback on top of the existing Stage 4 engine.

#### Tasks
1. Refactor playback timing/scrubbing out of `pitch_renderer.gd` into `match_playback_controller.gd` so both `PitchView2D` and `Pitch3D` receive the exact same tick state, speed multiplier (`1x/2x/4x/8x`), pause state, and rollback notifications.
2. Implement `pitch_surface.gdshader` on a $125\text{ m} \times 84\text{ m}$ `PlaneMesh` with all exact FIFA/`fm_core` pitch lines and mowed stripes.
3. Build `pitch_3d.tscn` with exact `HomeGoal` and `AwayGoal` geometry (`7.32m x 2.44m`), `BallRoot` (textured rolling sphere + dynamic height ground shadow), and 22 placeholder capsule `Player3D` instances with facing cones and distinct Home/Away/GK colors.
4. Implement `camera_rig_3d.gd` with Broadcast (`F1`), Tactical 3D (`F2`), Behind Goal (`F3`), Free Orbit (`F4`), and 2D Classic (`F5`) switching, plus HUD buttons.
5. Verify that mid-match tactical rollback (`rollback_and_resimulate`) updates both 2D and 3D views cleanly without stale frames.

#### Definition of Done (DoD)
* Existing `ctest` suite passes with 0 modifications to `core/` or `gdextension/`.
* Godot headless smoke test (`--headless --path godot_client --quit-after 30`) exits cleanly with 0 script or shader errors.
* Toggling `F1`–`F5` mid-match switches smoothly between 3D cameras and the existing 2D Classic view with identical player/ball positions (e.g., ball crossing `X = ±52.5, |Y| < 3.66, Z < 2.44` visually enters the 3D goal net).
* Scrubbing the timeline, pausing, stepping single frames, and triggering a Stage 3 tactical rollback all work identically in 3D and 2D.

---

### Stage 4.5B: Event Look-Ahead Bridge, 3D Character Rig & `AnimationTree`
**Scope:** Add read-only packed event streaming from GDExtension, build the look-ahead animation pipeline, and support full skinned 3D humanoid characters with seamless fallback.

#### Tasks
1. Add `MatchEngineBridge::get_events_packed()` in `gdextension/` returning the 6-float-per-event `PackedFloat32Array` and bind it via `_bind_methods()`.
2. Implement `animation_director.gd` reading `get_events_packed()` on match load and after rollback, firing look-ahead triggers for `Pass`, `Shoot`, `TackleWon`, `Save` (left/right dive), and `Goal` (celebration), with safe cursor reset on scrubbing/rollback.
3. Build `player_3d.tscn`, `player_visual_3d.gd`, and `player_kit.gdshader`:
   * Compute smoothed ground velocity from frame deltas to drive `AnimationTree` `BlendSpace1D` (`Idle/Walk/Jog/Sprint`) and stride `TimeScale`.
   * Support `AnimationNodeOneShot` overrides for kicks, tackles, GK dives, and celebrations.
   * Automatically bind `res://assets/characters/footballer.glb` when present; fall back gracefully to an animated procedural capsule rig (leaning into sprints, kicking leg/pulse indicator, GK dive tilt) if the `.glb` is not yet provided.
4. Add a concise `godot_client/assets/README_ASSETS.md` documenting the exact Mixamo/glTF export & Godot 4 retargeting checklist (clip names, `+Z` forward offset, bone map).

#### Definition of Done (DoD)
* CMake build + `ctest` pass with 0 warnings; `fm_core` determinism and Stage 4 `fm_sim_cli` numbers remain byte-identical.
* `get_events_packed()` returns valid chronological events matching the HUD log, and refreshes properly after `rollback_and_resimulate()`.
* Players visibly transition smoothly between Idle, Jog, and Sprint matching ground speed, and trigger kick/tackle/dive animations 4–5 ticks *before* ball release/save impact.
* Scrubbing backward/forward across goals does not leave players stuck in dive/celebration poses or double-fire stale animations.

---

### Stage 4.5C: Stadium Atmosphere, MultiMesh Crowd & Matchday Polish
**Scope:** Surround the pitch with a believable stadium environment and matchday readability polish while keeping 60+ FPS performance.

#### Tasks
1. Implement `stadium_builder_3d.gd`: load `res://assets/stadium/stadium.glb` if present, otherwise procedurally generate 4 tiered stands, corner floodlights, and pitch-side boards outside the $125\text{ m} \times 84\text{ m}$ apron.
2. Add single-draw-call `MultiMeshInstance3D` crowd (2,500+ spectators) with `crowd_spectator.gdshader` supporting Home/Away color zones and a goal/shot excitement pulse triggered by `AnimationDirector`.
3. Configure `WorldEnvironment` (procedural daylight or floodlit evening toggle), soft directional shadows, `BallCarrierRing` ground indicator, and optional toggleable 3D player labels (`L` hotkey).
4. Add subtle goal celebration camera focus (brief 2-second zoom toward scorer during `PlayState::GoalScored`) that respects user manual camera override and scrubbing.

#### Definition of Done (DoD)
* Godot headless smoke test passes cleanly with 0 errors.
* Full 3D match runs smoothly at 60 FPS with stadium, shadows, 22 animated players, and 2,500+ MultiMesh spectators active.
* Scoring a goal visibly triggers scorer celebration, crowd excitement shader pulse, and clean reset back to kickoff positioning without camera glitches.

---