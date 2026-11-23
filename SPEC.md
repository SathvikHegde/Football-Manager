# **Football Manager Clone: Architecture & Initial Build Specification (LLM Master Blueprint)**

# **1\. Purpose & Strict Engineering Rules for the Coding LLM**

This document serves as the authoritative, low-level engineering specification for bootstrapping a deep, deterministic, Football Manager-style football simulation engine. It is specifically designed to be parsed and executed by AI coding agents to build Stages 0 through 4 cleanly, modularly, and without design regressions.

The AI assistant must obey the following strict engineering constraints at all times:

* **Zero Engine Coupling in Core:** The `core/` static library must be written in pure C++20 with zero Godot headers, zero graphics/GUI dependencies, and zero OS-specific API calls. It must compile with any standard C++20 compiler (`g++`, `clang++`, `MSVC`).  
* **Strict Determinism:** All simulation randomness must flow strictly through an explicit, seeded PRNG (`uint64_t` seed, using PCG32 or `std::mt19937_64`) stored inside `MatchState`. Do not use `std::rand()`, `std::random_device`, system wall-clock time, or pointer-address hashing. Never iterate over un-ordered associative containers (`std::unordered_map`) when modifying simulation state. Given identical seeds, team data, and tactical configurations, two execution runs must produce byte-identical `MatchRecording` outputs across all supported operating systems.  
* **Pre-Computed Simulation, Dumb Renderer:** The C++ backend simulates the match headlessly into a contiguous keyframe and event buffer (`MatchRecording`). Godot 4 acts exclusively as a replay engine and UI shell that interpolates between visual keyframes and renders interface elements.  
* **No Per-Entity Per-Tick FFI Calls:** Never expose granular, individual getter calls (e.g., `get_player_x(player_index)`) across the Foreign Function Interface (FFI) boundary every frame. Stream packed primitive buffers (`PackedFloat32Array`) containing frame data across the C++/Godot GDExtension boundary.  
* **Stage-Gated Execution:** Work must strictly follow the defined stages (Stage 0 through Stage 4). Each stage must compile cleanly, pass its automated verification suite, and produce a functioning artifact before code generation for subsequent stages begins.  
* **Avoid Premature Complexity:** Do not introduce 3D skeletal animations, complex transfer market logic, scouting networks, granular financial models, or 35+ attributes until Stage 4 passes all statistical calibration baselines.

---

# **2\. Technology Stack & Architecture Overview**

The system architecture is strictly decoupled into five functional layers, separating simulation, testing, FFI binding, and presentation.

| Layer | Technical Identity | Primary Responsibility |
| :---- | :---- | :---- |
| **Core Simulation Library** | `fm_core` (C++20 Static Library) | Encapsulates math utilities, pitch geometry, ball/player physics, Utility AI decision-making, tactics, match rules, and frame/event logging. Zero external dependencies beyond the C++ STL. |
| **Headless Calibration CLI** | `fm_sim_cli` (C++20 Executable) | Standalone console application linking `fm_core`. Executes batch simulations (100–1,000 matches) headlessly to output aggregate statistical telemetry (CSV and console reports). |
| **Automated Test Suite** | `fm_tests` (C++20 Catch2 / GTest) | Automated unit testing framework validating determinism, physics boundaries, snapshot rollback equivalence, and match state rules. |
| **Godot 4 Bridge** | `fm_gdextension` (C++20 Dynamic Library) | Thin FFI wrapper built using Godot 4 **GDExtension** (`godot-cpp`). Links `fm_core` and exposes the `MatchEngineBridge` object to GDScript. |
| **Frontend Client** | `godot_client` (Godot 4.4+ Engine Project) | Presentation layer implementing 2D top-down pitch visualizer (classic FM dots), ball height/shadow rendering, timeline playback controls, tactical panels, and commentary feeds. |

---

# **3\. Repository & Directory Structure**

The repository must follow a clean directory hierarchy separating C++ backend core modules, binding layers, and Godot frontend assets.

```
football_manager/
├── CMakeLists.txt
├── core/
│   ├── include/
│   │   └── fm/
│   │       ├── ai/
│   │       │   ├── execution.hpp
│   │       │   ├── goalkeeper_ai.hpp
│   │       │   ├── tactical_anchors.hpp
│   │       │   ├── team_phase.hpp
│   │       │   └── utility_ai.hpp
│   │       ├── engine/
│   │       │   ├── ball_physics.hpp
│   │       │   ├── match_simulator.hpp
│   │       │   ├── pitch.hpp
│   │       │   ├── player_kinematics.hpp
│   │       │   └── rules.hpp
│   │       ├── math/
│   │       │   ├── curves.hpp
│   │       │   ├── rng.hpp
│   │       │   ├── vec2.hpp
│   │       │   └── vec3.hpp
│   │       ├── model/
│   │       │   ├── attributes.hpp
│   │       │   ├── match_event.hpp
│   │       │   ├── match_state.hpp
│   │       │   ├── player.hpp
│   │       │   ├── tactics.hpp
│   │       │   └── team.hpp
│   │       └── recording/
│   │           ├── checkpoint_manager.hpp
│   │           ├── frame_snapshot.hpp
│   │           └── match_recording.hpp
│   └── src/
│       ├── ai/
│       ├── engine/
│       ├── math/
│       ├── model/
│       └── recording/
├── cli_harness/
│   └── main.cpp
├── tests/
│   ├── test_determinism.cpp
│   ├── test_physics.cpp
│   ├── test_rollback.cpp
│   └── test_rules.cpp
├── gdextension/
│   ├── CMakeLists.txt
│   ├── SConstruct
│   ├── src/
│   │   ├── match_engine_bridge.cpp
│   │   └── register_types.cpp
│   └── include/
│       └── match_engine_bridge.hpp
└── godot_client/
    ├── project.godot
    ├── fm.gdextension
    ├── scenes/
    │   ├── main.tscn
    │   ├── pitch_view.tscn
    │   └── ui/
    └── scripts/
        ├── match_hud.gd
        └── pitch_renderer.gd
```

## **Dependency Contracts**

1. `core/` depends **ONLY** on the standard C++ library (C++20). It has zero knowledge of Godot, OS windowing, or CMake targets outside itself.  
2. `cli_harness/`, `tests/`, and `gdextension/` depend on `core/`.  
3. `godot_client/` interacts with C++ logic strictly via `gdextension/` bindings (`MatchEngineBridge`).

---

# **4\. Coordinate System, Constants & Core Data Contracts**

## **4.1 Coordinate System & Pitch Geometry**

All simulation units use standard SI metric units (meters, seconds, meters/second, radians).

* **Axis Orientation:** Origin `(0.0, 0.0, 0.0)` is at the exact geometric center of the pitch ground plane.  
  * `X-axis`: Pitch Length (`-52.5m` to `+52.5m`). Home team attacks toward `+X` in the first half.  
  * `Y-axis`: Pitch Width (`-34.0m` to `+34.0m`).  
  * `Z-axis`: Elevation/Height (`0.0m` at pitch surface, positive upwards).  
* **Goal Dimensions:**  
  * Width: `7.32m` (Y range: `[-3.66m, +3.66m]`)  
  * Height: `2.44m` (Z range: `[0.0m, +2.44m]`)  
  * Goal line locations: Home Goal at `X = -52.5m`, Away Goal at `X = +52.5m`.  
* **Key Pitch Boundaries:**  
  * Penalty Area Length: `16.5m` from goal line.  
  * Penalty Area Width: `40.32m` (Y range: `[-20.16m, +20.16m]`).  
  * Center Circle Radius: `9.15m`.  
* **Simulation Timing:**  
  * Physics tick rate: `20 Hz` (`DT = 0.05s`).  
  * First / Second Half Ticks: `45 min * 60 s * 20 Hz = 54,000 ticks` each.  
  * Full Regulation Match: `108,000 ticks` (excluding added stoppage time).  
  * AI Decision Cadence: Every `4 ticks` (`0.20s`), evaluated per player with staggered tick offsets, or triggered immediately via state interrupts (e.g., possession change, loose ball entering control zone, set piece execution).

## **4.2 Data Structures & C++ Interfaces**

```
// core/include/fm/math/vec2.hpp
namespace fm {
struct Vec2 {
    float x{0.0f};
    float y{0.0f};
    
    constexpr Vec2() = default;
    constexpr Vec2(float x_, float y_) : x(x_), y(y_) {}
    
    constexpr Vec2 operator+(const Vec2& o) const { return {x + o.x, y + o.y}; }
    constexpr Vec2 operator-(const Vec2& o) const { return {x - o.x, y - o.y}; }
    constexpr Vec2 operator*(float scalar) const { return {x * scalar, y * scalar}; }
    
    [[nodiscard]] float length_sq() const { return x * x + y * y; }
    [[nodiscard]] float length() const { return std::sqrt(length_sq()); }
    [[nodiscard]] Vec2 normalized() const {
        float len = length();
        return len > 0.0001f ? Vec2{x / len, y / len} : Vec2{0.0f, 0.0f};
    }
};
}

// core/include/fm/math/vec3.hpp
namespace fm {
struct Vec3 {
    float x{0.0f};
    float y{0.0f};
    float z{0.0f};
    
    constexpr Vec3() = default;
    constexpr Vec3(float x_, float y_, float z_) : x(x_), y(y_), z(z_) {}
    
    constexpr Vec3 operator+(const Vec3& o) const { return {x + o.x, y + o.y, z + o.z}; }
    constexpr Vec3 operator-(const Vec3& o) const { return {x - o.x, y - o.y, z - o.z}; }
    constexpr Vec3 operator*(float scalar) const { return {x * scalar, y * scalar, z * scalar}; }
    
    [[nodiscard]] float length_sq() const { return x * x + y * y + z * z; }
    [[nodiscard]] float length() const { return std::sqrt(length_sq()); }
    [[nodiscard]] Vec3 normalized() const {
        float len = length();
        return len > 0.0001f ? Vec3{x / len, y / len, z / len} : Vec3{0.0f, 0.0f, 0.0f};
    }
};
}

// core/include/fm/model/attributes.hpp
namespace fm {
struct PlayerAttributes {
    uint8_t pace{10};          // Movement top speed modifier
    uint8_t acceleration{10};  // Acceleration rate modifier
    uint8_t stamina{10};       // Fatigue drop-off rate
    uint8_t passing{10};       // Pass directional accuracy & variance
    uint8_t vision{10};        // Max effective pass evaluation distance & search cone angle
    uint8_t shooting{10};      // Shot accuracy, velocity control, xG execution
    uint8_t dribbling{10};     // Ball control tightness & speed while carrying
    uint8_t tackling{10};      // Tackle reach & success probability
    uint8_t positioning{10};   // Off-ball defensive anchor compliance
    uint8_t decisions{10};     // Utility AI noise reduction / optimal action choice rate
    
    [[nodiscard]] constexpr float norm(uint8_t val) const {
        return static_cast<float>(std::clamp<uint8_t>(val, 1, 20) - 1) / 19.0f;
    }
};
}

// core/include/fm/model/player.hpp
namespace fm {
enum class ActionType : uint8_t {
    Idle, Move, Pass, Dribble, Shoot, Tackle, Intercept, Shield, Save
};

enum class RoleId : uint8_t {
    Goalkeeper, CenterBack, FullBack, Midfielder, Winger, Forward
};

struct PlayerAgent {
    uint8_t slot_index{0};           // 0..21 (0..10 Home, 11..21 Away; 0 and 11 are GKs)
    uint32_t player_id{0};
    uint8_t team_id{0};              // 0 = Home, 1 = Away
    RoleId role{RoleId::Midfielder};
    Vec2 position{0.0f, 0.0f};
    Vec2 velocity{0.0f, 0.0f};
    float facing_rad{0.0f};
    float stamina_pct{1.0f};         // 1.0 (fully rested) -> 0.0 (exhausted)
    ActionType current_action{ActionType::Idle};
    uint16_t action_cooldown_ticks{0};
    Vec2 move_target{0.0f, 0.0f};
    PlayerAttributes attrs{};
};
}

// core/include/fm/model/match_state.hpp
namespace fm {
struct BallState {
    Vec3 position{0.0f, 0.0f, 0.0f};
    Vec3 velocity{0.0f, 0.0f, 0.0f};
    Vec3 spin_rad_s{0.0f, 0.0f, 0.0f};
    int8_t owner_slot{-1};           // -1 if loose in flight/ground
    int8_t last_touch_slot{-1};
    int8_t last_touch_team{-1};
    uint16_t claim_lock_ticks{0};    // Prevents instant re-claim by passer
};

enum class FormationId : uint8_t {
    F_442, F_433, F_4231, F_352, F_532
};

struct TacticalConfig {
    FormationId formation{FormationId::F_442};
    std::array<Vec2, 11> base_anchors_norm{}; // Normalized [0,1] pitch positions
    float defensive_line{0.5f};     // 0.0 (Deep Drop) to 1.0 (High Line)
    float pressing_intensity{0.5f}; // 0.0 (Stand Off) to 1.0 (Gegenpress)
    float passing_directness{0.5f}; // 0.0 (Short/Tiki-Taka) to 1.0 (Direct/Long Ball)
    float tempo{0.5f};              // 0.0 (Patient) to 1.0 (Fast Pace)
};

enum class PlayState : uint8_t {
    Kickoff, OpenPlay, ThrowIn, GoalKick, Corner, GoalScored, HalfTime, FullTime
};

struct RngState {
    uint64_t state{0x853c49e6748fea9bULL};
    uint64_t inc{0xda3e39cb94b95bdbULL};
    
    uint32_t next_u32() {
        uint64_t oldstate = state;
        state = oldstate * 6364136223846793005ULL + (inc | 1);
        uint32_t xorshifted = static_cast<uint32_t>(((oldstate >> 18u) ^ oldstate) >> 27u);
        uint32_t rot = static_cast<uint32_t>(oldstate >> 59u);
        return (xorshifted >> rot) | (xorshifted << ((~rot + 1u) & 31u));
    }
    
    float next_float() { // Returns [0.0, 1.0)
        return static_cast<float>(next_u32()) / 4294967296.0f;
    }
    
    float next_gaussian(float mean, float stddev) {
        float u1 = std::max(1e-7f, next_float());
        float u2 = next_float();
        float z0 = std::sqrt(-2.0f * std::log(u1)) * std::cos(6.28318530718f * u2);
        return mean + z0 * stddev;
    }
};

struct MatchState {
    uint32_t tick{0};
    uint8_t half{1};
    uint8_t score_home{0};
    uint8_t score_away{0};
    PlayState play_state{PlayState::Kickoff};
    uint16_t restart_timer_ticks{0};
    int8_t restart_team{-1};
    Vec2 restart_spot{0.0f, 0.0f};
    BallState ball{};
    std::array<PlayerAgent, 22> players{};
    TacticalConfig home_tactics{};
    TacticalConfig away_tactics{};
    RngState rng{};
};
}

// core/include/fm/model/match_event.hpp
namespace fm {
enum class EventType : uint8_t {
    Kickoff, Pass, PassComplete, PassIntercepted, Dribble, TackleWon,
    Shot, ShotOnTarget, Save, Goal, OutTouchline, OutGoalLine,
    Corner, GoalKick, ThrowIn, HalfTime, FullTime, TacticalChange
};

struct MatchEvent {
    uint32_t tick{0};
    EventType type{EventType::Kickoff};
    int8_t primary_slot{-1};
    int8_t secondary_slot{-1};
    Vec2 location{0.0f, 0.0f};
    float xg_value{0.0f};
};
}
```

---

# **5\. Match Engine Simulation Pipeline (4-Layer Tick Architecture)**

Every physics step (`MatchSimulator::step_tick(MatchState& state)` at `DT = 0.05s`) executes strictly sequentially through four architecture layers:

```
+-----------------------------------------------------------------------+
| LAYER 1: MATCH FLOW, CLOCK & RULES                                    |
| - Clock Progression, Half-Time/Full-Time Transitions                  |
| - Boundary & Goal Checking, Set-Piece State Machine Transitions       |
+-----------------------------------------------------------------------+
                                   |
                                   v
+-----------------------------------------------------------------------+
| LAYER 2: TEAM PHASE & DYNAMIC TACTICAL ANCHORS                        |
| - Phase Classification (InPossession, OutOfPossession, Transitions)   |
| - Compute Dynamic Player Anchors Shifted by Ball Position & Tactics   |
+-----------------------------------------------------------------------+
                                   |
                                   v
+-----------------------------------------------------------------------+
| LAYER 3: AGENT UTILITY AI                                             |
| - Evaluated Every 4 Ticks per Player (Staggered Offsets or Interrupt) |
| - On-Ball Utility: Pass, Shoot, Dribble, Shield                       |
| - Off-Ball Utility: Press, Support Space, Cover Anchor                |
| - Goalkeeper AI: Positioning Bisector & Shot Intercept Dive           |
+-----------------------------------------------------------------------+
                                   |
                                   v
+-----------------------------------------------------------------------+
| LAYER 4: PHYSICS, KINEMATICS & ATTRIBUTE-DRIVEN EXECUTION             |
| - Pass/Shot Execution Variance (Gaussian Angular & Power Noise)       |
| - 3D Ball Physics: Gravity, Drag, Bounce, Turf Friction               |
| - 2D Player Kinematics: Speed Scaling, Accel Limits, Turn Rate Clamp  |
| - Possession Radius Resolution & Contested Tackling Logic             |
+-----------------------------------------------------------------------+
```

## **Layer 1: Match Flow, Clock & Rules**

1. Advance `state.tick++`. Handle state boundaries at `tick == 54000` (Half-Time: switch attack directions, reset player positions to half pitch, set `PlayState::HalfTime`) and `tick == 108000` (Full-Time: `PlayState::FullTime`).  
2. Evaluate Goal Line Crossing: If `|ball.pos.x| > 52.5` while `|ball.pos.y| < 3.66` and `ball.pos.z < 2.44`:  
   * Increment home or away score.  
   * Emit `EventType::Goal`.  
   * Set `state.play_state = PlayState::GoalScored`. Set `restart_timer_ticks = 60` (3 seconds delay), leading to `PlayState::Kickoff`.  
3. Evaluate Pitch Out-of-Bounds (when ball is on ground or below clear height):  
   * `|ball.pos.y| > 34.0`: Trigger `PlayState::ThrowIn`. Restarting team is opposite of `ball.last_touch_team`.  
   * `|ball.pos.x| > 52.5` (outside goal width): Trigger `PlayState::Corner` (if last touched by defending team) or `PlayState::GoalKick` (if last touched by attacking team).  
4. Set-Piece Execution Engine (Stages 2–3 simplify set pieces): Place ball at restart spot. Move taker to ball, hold remaining 21 players outside a `9.15m` exclusion zone around the spot. Force immediate pass/kick state after `restart_timer_ticks` reaches zero.

## **Layer 2: Team Phase & Dynamic Tactical Anchors**

Each team's collective phase is classified based on ball possession:

* `InPossession`: Team owns ball or ball is in flight originating from team pass.  
* `OutOfPossession`: Opponent owns ball or ball is in flight from opponent.  
* `AttackingTransition` / `DefensiveTransition`: Ball is loose within 3 seconds of possession loss/gain.

### **Anchor Calculations**

Player positioning uses dynamic anchor targets calculated from base formation normalized coordinates `(norm_x, norm_y) in [0.0, 1.0]^2`:

\$\$\\text{anchor\_x} \= (\\text{norm\_x} \- 0.5) \\times 105.0\$\$

\$\$\\text{anchor\_y} \= (\\text{norm\_y} \- 0.5) \\times 68.0\$\$

During play, anchor\_pos shifts relative to the ball position \$(B\_x, B\_y)\$ and team tactical parameters:

\$\$\\text{shift\_x} \= B\_x \\times 0.35 \\times \\text{defensive\_line}\$\$

\$\$\\text{shift\_y} \= B\_y \\times 0.25\$\$

\$\$\\text{dynamic\_anchor} \= \\text{Vec2}(\\text{anchor\_x} \+ \\text{shift\_x}, \\text{anchor\_y} \+ \\text{shift\_y})\$\$

Out of possession, team width compresses by \$15%\$. In possession, wingers extend width toward touchlines (\$\\pm 30.0\\text{m}\$).

## **Layer 3: Agent Utility AI**

Every agent evaluates available candidate actions on a normalized scale \$\[0.0, 1.0\]\$. The selected action is the candidate with the highest Utility Score after applying decision-error noise:

\$\$\\text{FinalScore} \= \\text{Score} \+ \\text{RNG.next\_gaussian}\\left(0.0, \\sigma\_{\\text{decisions}}\\right)\$\$

\$\$\\sigma\_{\\text{decisions}} \= 0.15 \\times (1.0 \- \\text{norm}(\\text{attrs.decisions}))\$\$

```
                               +-----------------------------+
                               | Player Has Ball Possession? |
                               +-----------------------------+
                                              |
                       +----------------------+----------------------+
                       | Yes                                         | No
                       v                                             v
        +------------------------------+              +------------------------------+
        | Evaluate On-Ball Utilities   |              | Evaluate Off-Ball Utilities  |
        | - Shoot                      |              | - Press Ball Carrier         |
        | - Pass (AllTeammates)        |              | - Cover Space / Mark Anchor  |
        | - Dribble (8 Rays)           |              | - Support Ball Carrier       |
        | - Hold / Shield              |              | - Make Forward Run           |
        +------------------------------+              +------------------------------+
                       |                                             |
                       +----------------------+----------------------+
                                              |
                                              v
                               +-----------------------------+
                               | Select Max Utility Action   |
                               | (Plus Noise from Decisions) |
                               +-----------------------------+
```

### **On-Ball Candidates**

1. **Shoot Utility:**  
   * Evaluates distance \$d\$ to goal center and angle \$\\theta\$ relative to posts.  
   * \$\\text{Utility}\_{\\text{Shoot}} \= \\text{norm}(\\text{shooting}) \\times \\cos(\\theta) \\times \\max\\left(0.0, 1.0 \- \\frac{d}{35.0}\\right)\$.  
   * If \$d \> 35.0\\text{m}\$ and goalkeeper is in position, utility is clamped to \$0.0\$.  
2. **Pass Utility (Evaluated for each candidate teammate \$j\$):**  
   * \$\\text{Utility}*{\\text{Pass}}(j) \= S*{\\text{openness}} \\times 0.4 \+ S\_{\\text{progression}} \\times 0.4 \+ S\_{\\text{distance}} \\times 0.2\$.  
   * \$S\_{\\text{openness}}\$: Distance from passing line segment to nearest opponent.  
   * \$S\_{\\text{progression}}\$: Forward gain toward opponent goal along X-axis.  
   * Filter out candidates if distance exceeds vision range: \$D\_{\\text{max}} \= 15.0 \+ 35.0 \\times \\text{norm}(\\text{vision})\$.  
3. **Dribble Utility (Evaluated across 8 radial direction vectors):**  
   * Score depends on open space along vector for \$8.0\\text{m}\$, alignment toward opponent goal, and relative `dribbling`/`pace` vs nearby defender `tackling`.  
4. **Hold / Shield Utility:**  
   * Base fallback score \$= 0.25\$. Increases up to \$0.7\$ if all pass lanes are blocked and a defender is within \$2.0\\text{m}\$.

### **Off-Ball Candidates**

* **Press Ball Carrier:** Assigned to the 1–2 closest defenders to the ball. Utility scales with `pressing_intensity` and inverse distance to ball. Prevents multi-player swarm defense.  
* **Support Ball Carrier:** Open passing space within \$8\\text{m}\$ to \$18\\text{m}\$ from carrier.  
* **Make Forward Run:** Attack space behind defensive line when teammate faces forward in possession.  
* **Cover Anchor:** Maintain position goal-side of dynamic anchor point.

### **Goalkeeper AI (Slots 0 & 11\)**

* **Positioning:** Remains on the angle bisector between the ball position and the two goalposts. Depth off goal line scales from \$1.5\\text{m}\$ (ball in box) to \$8.0\\text{m}\$ (ball at midfield).  
* **Shot Reaction & Diving:** When a shot is fired, GK calculates the arrival point at the goal plane \$X \= \\pm 52.5\$.  
  * Required dive distance \$D\_{\\text{dive}} \= \|\\text{arrival\_pos} \- \\text{gk\_pos}\|\$.  
  * Available time \$T\_{\\text{flight}} \= \\frac{D\_{\\text{shot}}}{\\text{shot\_speed}}\$.  
  * Reaction time \$T\_{\\text{react}} \= 0.25\\text{s} \- 0.12\\text{s} \\times \\text{norm}(\\text{positioning})\$.  
  * Max physical reach \$R \= (T\_{\\text{flight}} \- T\_{\\text{react}}) \\times (4.5 \+ 4.0 \\times \\text{norm}(\\text{acceleration}))\$.  
  * If \$R \\ge D\_{\\text{dive}}\$, shot is saved (`Save`). Deflection vector is determined by \$R \- D\_{\\text{dive}}\$. Otherwise, emit `Goal`.

## **Layer 4: Physics, Kinematics & Attribute-Driven Execution**

### **Execution Variance (Attribute-Driven Accuracy)**

Actions output idealized target velocity vectors \$\\vec{V}*{\\text{ideal}}\$. Execution introduces angular error \$\\theta*{\\text{err}}\$ and power multiplier \$P\_{\\text{err}}\$ sampled from the deterministic PRNG:

\$\$\\theta\_{\\text{err}} \\sim \\text{Gaussian}\\left(0.0, \\sigma\_{\\theta}\\right)\$\$

\$\$\\sigma\_{\\theta} \= \\sigma\_{\\text{base}} \\times (1.0 \- 0.7 \\times \\text{norm}(\\text{attribute})) \\times (1.0 \+ 0.5 \\times \\text{pressure}) \\times (2.0 \- \\text{stamina\_pct})\$\$

where \$\\sigma\_{\\text{base}} \= 0.15\\text{ rad}\$ (\$\\approx 8.6^\\circ\$) for passing, \$0.22\\text{ rad}\$ (\$\\approx 12.6^\\circ\$) for shooting. `pressure` is \$1.0\$ if an opponent is within \$2.5\\text{m}\$, down to \$0.0\$ if open.

### **3D Ball Physics Engine**

Update equations applied at \$DT \= 0.05\\text{s}\$:

\$\$\\vec{a}{\\text{ball}} \= \\begin{bmatrix} 0 \\ 0 \\ \-9.81 \\end{bmatrix} \- C{\\text{drag}} \|\\vec{v}\| \\vec{v}\$\$

where drag coefficient \$C\_{\\text{drag}} \= 0.006\$.

When \$\\text{pos.z} \\le 0.0\$:

* Vertical Bounce: \$v\_z \= \-v\_z \\times e\_{\\text{restitution}}\$ (\$e\_{\\text{restitution}} \= 0.55\$).  
* Ground Friction Damping: \$v\_x \= v\_x \\times 0.94\$, \$v\_y \= v\_y \\times 0.94\$.  
* If \$\|v\_z\| \< 0.15\\text{ m/s}\$, set \$v\_z \= 0.0\$ and \$\\text{pos.z} \= 0.0\$.

### **2D Player Kinematics Engine**

Players accelerate toward their target velocity \$\\vec{V}\_{\\text{target}}\$:

\$\$V\_{\\text{max}} \= \\left(6.0 \+ 3.2 \\times \\text{norm}(\\text{pace})\\right) \\times (0.7 \+ 0.3 \\times \\text{stamina\_pct})\$\$

\$\$\\text{Accel}\_{\\text{max}} \= 2.5 \+ 2.5 \\times \\text{norm}(\\text{acceleration})\\quad \[\\text{m/s}^2\]\$\$

\$\$\\vec{a} \= \\text{Clamp}\\left(\\frac{\\vec{V}*{\\text{target}} \- \\vec{V}*{\\text{current}}}{DT}, \\text{Accel}\_{\\text{max}}\\right)\$\$

\$\$\\vec{V}*{\\text{new}} \= \\vec{V}*{\\text{current}} \+ \\vec{a} \\times DT\$\$

Turn rate is clamped to \$\\omega\_{\\text{max}} \= 8.0\\text{ rad/s}\$ to enforce realistic movement inertia.

### **Possession & Tackling**

* **Ball Control:** If ball height \$Z \< 1.8\\text{m}\$, distance to player \$\< 0.9\\text{m}\$, and `claim_lock_ticks == 0`, player takes possession (`owner_slot = player_slot`).  
* **Tackling:** If defender comes within \$1.2\\text{m}\$ of ball carrier:  
    
  \$\$P\_{\\text{tackle\_success}} \= 0.50 \+ 0.35 \\times \\text{norm}(\\text{tackling}) \- 0.30 \\times \\text{norm}(\\text{dribbling\_carrier})\$\$  
    
  Sample PRNG vs \$P\_{\\text{tackle\_success}}\$:  
    
  * **Success:** Carrier loses ball (`owner_slot = -1`), ball receives deflection impulse, carrier enters 0.5s stun cooldown.  
  * **Failure:** Defender enters 0.6s recover cooldown; carrier retains ball.

---

# **6\. Recording, Snapshot Rollback & GDExtension Streaming Protocol**

## **6.1 Flat Frame Wire Format (Packed Buffer)**

To eliminate FFI boundary overhead, simulation frames are packed into a contiguous `PackedFloat32Array` containing exactly 93 floats per frame (`FLOATS_PER_FRAME = 93` \= 372 bytes).

```
[0]: Frame Tick (float)
[1]: Ball Position X
[2]: Ball Position Y
[3]: Ball Position Z
[4]: Ball Owner Slot Index (-1.0 if loose)
--------------------------------------------------------------
[5 + i*4 + 0]: Player i Position X    (i = 0..21)
[5 + i*4 + 1]: Player i Position Y
[5 + i*4 + 2]: Player i Facing Angle (Radians)
[5 + i*4 + 3]: Player i State Flags / Stamina %
```

## **6.2 C++ GDExtension Bridge Interface**

```
// gdextension/include/match_engine_bridge.hpp
#pragma once
#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/variant/packed_float32_array.hpp>
#include <godot_cpp/variant/dictionary.hpp>
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
    ~MatchEngineBridge();

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
```

## **6.3 Checkpoint & Tactical Rollback Engine**

* **Checkpoint Intervals:** Full-state copies of `MatchState` are stored every 1,200 ticks (1 match minute, \~90 total checkpoints \= \~250 KB RAM per match).  
* **Rollback Procedure (When user changes tactics at tick \$T\_{\\text{user}}\$):**  
  1. Retrieve nearest checkpoint state \$C\$ where \$T\_{\\text{checkpoint}} \\le T\_{\\text{user}}\$.  
  2. Overwrite `current_state_` with snapshot \$C\$.  
  3. Fast-forward silent simulation loop from \$T\_{\\text{checkpoint}}\$ up to \$T\_{\\text{user}}\$ without logging output.  
  4. Apply requested tactical configuration mutation (`home_tactics` or `away_tactics`).  
  5. Truncate `MatchRecording` frame buffer at index \$T\_{\\text{user}}\$.  
  6. Resume standard pre-simulation from \$T\_{\\text{user}}\$ to full-time (\$108,000\$ ticks).

```
Timeline: 0m ------------- 45m -------- 47m (User Edits Tactics) -------- 90m
                             |              |
                      Checkpoint @ 45m      |
                             |              |
                             +== Fast-Fwd ==+
                                            | (Apply Mutation & Truncate Buffer)
                                            +== Resimulate to 90m ============>
```

---

# **7\. Phased Implementation Plan (Stages 0–4)**

## **Stage 0: Repository, Build System & GDExtension Hello-World**

### **Tasks**

1. Construct complete repository tree layout.  
2. Configure root `CMakeLists.txt` compiling `fm_core` as a static library with target C++20 standard. Include basic C++ test framework (Catch2 or GTest).  
3. Set up GDExtension build environment using `godot-cpp` targeting Godot 4.4+.  
4. Create `MatchEngineBridge` class exposing `ping()` returning `"fm_core C++20 engine ready"`.

### **Definition of Done (DoD)**

* `cmake --build build && ctest` succeeds cleanly with 0 warnings.  
* Godot 4.4 editor opens `godot_client/`, loads `fm.gdextension`, and runs a test scene printing the engine version string to the console output.

---

## **Stage 1: Headless Physics Sandbox & 2D Pitch Viewer**

### **Tasks**

1. Implement math primitives (`Vec2`, `Vec3`, `RngState`) and `Pitch` dimensions.  
2. Implement 3D `BallPhysics` (drag, gravity, turf friction, bounce) and 2D `PlayerKinematics`.  
3. Implement snapshot frame storage (`FrameSnapshot`, `MatchRecording`).  
4. Implement `MatchEngineBridge::run_sandbox_demo()` populating 1,000 ticks of circular player movement and ball bounces.  
5. Build Godot `PitchRenderer` script drawing the field surface, boundary lines, 22 colored dots with jersey numbers, facing directional vectors, and elevated ball shadow projection. Add play/pause, scrub slider, and speed control toggles (\$1\\times, 2\\times, 4\\times, 8\\times\$).

### **Definition of Done (DoD)**

* Visual pitch display renders cleanly in Godot 2D at 60 FPS.  
* Ball bounces realistically with turf friction; players move along planned target vectors with correct rotational orientation and elevation shadows.

---

## **Stage 2: Playable 22-Player Match Loop & Minimal Utility AI**

### **Tasks**

1. Implement full `PlayerAttributes` structure and `MatchState`.  
2. Build Layer 2 Dynamic Anchors and Layer 3 Utility AI (Pass, Shoot, Dribble, Shield, Press, Cover Space).  
3. Implement Goalkeeper bisector positioning and dive intercept calculations.  
4. Build Layer 1 Match Rules state machine (Kickoff, Out-of-Bounds, Goal Kick, Corner, Goal Scored, Half-Time, Full-Time).  
5. Build Godot `MatchHUD` displaying score line, time display (`MM:SS`), active play state, and event text log output.

### **Definition of Done (DoD)**

* Headless engine runs a full 90-minute match (108,000 ticks) without crashes, deadlocks, stuck ball conditions, or out-of-bounds state locks.  
* Players execute passes, dribbles, pressing movements, defensive blocks, and shot attempts targeting goals cleanly.

---

## **Stage 3: Deterministic Pre-Simulation, Timeline Scrubber & Tactical Rollback**

### **Tasks**

1. Implement complete match pre-computation pipeline on startup (`MatchEngineBridge::simulate_full_match()`).  
2. Implement `CheckpointManager` capturing state every 1,200 ticks.  
3. Add GDExtension rollback API `rollback_and_resimulate(target_tick)`.  
4. Connect Godot timeline UI to jump to arbitrary frame ticks and render immediate playback.  
5. Create automated C++ unit tests in `tests/test_determinism.cpp` and `tests/test_rollback.cpp`.

### **Definition of Done (DoD)**

* **Determinism Test:** Executing two full match simulations using seed `0xDEADBEEF` produces identical 93-float array representations for all 108,000 frames.  
* **Rollback Test:** Pausing at minute 60, applying an identical tactical change, and resimulating produces output byte-identical to the original run. Mutating tactics (e.g., switching from low block to high press) produces divergent frame histories from minute 60 onward.

---

## **Stage 4: Headless Calibration Harness & Statistical Validation**

### **Tasks**

1. Build `cli_harness/main.cpp` (`fm_sim_cli`) parsing arguments:

```
./fm_sim_cli --matches 200 --threads 8 --home-rating 12 --away-rating 12 --out report.csv
```

2. Implement multi-threaded headless batch runner using `std::thread` / `std::async` running independent match seeds.  
3. Generate CSV metrics report and summary terminal tables logging statistical outputs.  
4. Tune Utility AI weights and variance curves until statistical metrics match target bands for equal-strength teams.

### **Statistical Calibration Targets (200-Match Batch Standard)**

| Metric | Target Range (Equal Teams) | Target Range (+4 Attribute Lead) |
| :---- | :---- | :---- |
| **Goals per Match** | `2.4 - 3.0` | `3.2 - 4.1` (Win % \$\>65%\$) |
| **Shots per Match** | `20.0 - 28.0` | `24.0 - 32.0` |
| **Shots on Target %** | `30% - 42%` | `35% - 48%` |
| **Pass Completion %** | `78% - 86%` | `82% - 89%` |
| **Possession Split** | `48% - 52%` | `58% - 66%` |

### **Definition of Done (DoD)**

* `fm_sim_cli` runs 200 matches in under 30 seconds across 8 thread workers.  
* Aggregate statistical metrics fall consistently within target calibration ranges across batch runs.

---