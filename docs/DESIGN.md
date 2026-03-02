# OpenAgora – Design Document

> Technical architecture, system design, and engineering decisions for the
> OpenAgora city simulation fork.

---

## 1. Current Architecture Audit

### 1.1 What exists today

The fork inherits Cytopia 0.4 – a working SDL2 isometric city builder.
The engine is solid but the *simulation layer is shallow*: zones grow buildings,
power grids connect, but there are no emergent economic feedback loops.

**Strengths to keep:**
- `TileData` already carries the right per-tile attributes (`inhabitants`,
  `happiness`, `educationLevel`, `pollutionLevel`, `crimeLevel`,
  `fireHazardLevel`, `upkeepCost`, `power`, `water`). The data model is ready.
- `GameClock` provides a dual real-time / game-time scheduler with configurable
  speed scaling – perfect for monthly simulation ticks.
- `Signal`/`SignalMediator` give a decoupled event bus. New simulation modules
  can publish events without touching the rendering code.
- `ZoneArea` already tracks power/water supply, vacancy, and density per zone.
  These are the right primitives for an affordability model.
- `MapLayers` bitmask system makes it trivial to add new overlay layers
  (e.g. the affordability heatmap) without changing core rendering.
- AngelScript integration provides a mod/scripting escape hatch.
- nlohmann/json is already used everywhere – data-driven content is one JSON
  file away.

**Weaknesses to address:**
- No city-wide aggregated indices (affordability, safety, etc.).
- No policy abstraction – all "policy" is currently implicit in tile placement.
- No event system – the game has no mechanism to react to emerging conditions.
- `GamePlay` is minimal: it just resets `ZoneManager` and `PowerManager`. There
  is no `GovernanceManager`, no `Economy` singleton.
- UI and simulation are not separated – build menu logic is tightly coupled to
  `TileData` rendering details.
- No feature flag system, so partially-implemented features ship silently.

### 1.2 Dependency Graph (current)

```
main.cxx
  └── Game
        ├── Map (render + mapNodes vector)
        │     └── MapNode (14 layers, TileData)
        ├── GamePlay
        │     ├── ZoneManager → ZoneArea
        │     └── PowerManager → PowerGrid
        ├── UIManager (ImGui orchestrator)
        │     └── BuildMenu, GameTimeMenu, PauseMenu, …
        ├── EventManager (SDL input)
        └── GameClock (tick scheduler)
              └── Randomizer, AudioMixer
```

`SignalMediator` sits as a global singleton connecting layers; `Settings`
and `TileManager` are additional singletons accessed across the graph.

---

## 2. Target Architecture

### 2.1 Separation of concerns

The goal is to make *simulation logic* independent of SDL so it can:
1. Be unit-tested without a display
2. Run headlessly for balance tuning / CI
3. Be swapped for a different renderer later

```
┌─────────────────────────────────────────────────────────┐
│  Presentation Layer                                     │
│  src/engine/   src/game/ui/                             │
│  SDL2 rendering, Dear ImGui panels, camera, input       │
└────────────────────┬────────────────────────────────────┘
                     │ reads indices, fires UI events
┌────────────────────▼────────────────────────────────────┐
│  Gameplay Layer                                         │
│  src/game/                                              │
│  ZoneManager, PowerManager, GovernanceManager           │
│  Orchestrates simulation modules per game-tick          │
└────────────────────┬────────────────────────────────────┘
                     │ calls tick(), reads TileData
┌────────────────────▼────────────────────────────────────┐
│  Simulation Layer  (NEW)                                │
│  src/simulation/                                        │
│  CityIndices, AffordabilityModel, PolicyEngine,         │
│  EventEngine – pure C++17, no SDL headers               │
└────────────────────┬────────────────────────────────────┘
                     │ reads from
┌────────────────────▼────────────────────────────────────┐
│  Data Layer                                             │
│  data/resources/data/                                   │
│  TileData.json, policies/*.json, events/*.json,         │
│  FeatureFlags.json                                      │
└─────────────────────────────────────────────────────────┘
```

### 2.2 New module inventory

| Module | Location | Responsibility |
|--------|----------|---------------|
| `FeatureFlags` | `src/services/` | Load `FeatureFlags.json`; gate in-dev systems |
| `CityIndices` | `src/simulation/` | Aggregate tile attributes → five scalar indices |
| `AffordabilityModel` | `src/simulation/` | Rent/income model, displacement pressure |
| `PolicyEngine` | `src/simulation/` | Load policy JSON; apply effects each tick |
| `EventEngine` | `src/simulation/` | Evaluate trigger conditions; fire events |
| `GovernanceManager` | `src/game/` | Public trust, council checkpoints, soft-fail |
| `CityIndicesPanel` | `src/game/ui/` | ImGui sidebar for five indices + trend |
| `PolicyPanel` | `src/game/ui/` | Sliders and toggles per active policy |
| `NotificationOverlay` | `src/game/ui/` | Toast-style event notifications |
| `AffordabilityOverlay` | `src/engine/render/` | Per-tile heatmap rendering |

### 2.3 New signals

Extend `SignalMediator` with:

```cpp
Signal::Signal<void(const CityIndices &)>  signalIndicesUpdated;
Signal::Signal<void(const GameEvent &)>    signalEventFired;
Signal::Signal<void(float publicTrust)>    signalTrustChanged;
Signal::Signal<void()>                     signalCouncilCheckpoint;
```

---

## 3. Feature Flag System

### 3.1 Motivation

Shipping half-finished simulation systems to a playable build causes confusion
and broken saves. Feature flags let us merge to `main` incrementally without
exposing unfinished systems to players.

### 3.2 Schema (`data/resources/data/FeatureFlags.json`)

```json
{
  "_comment": "Set to true to enable in-development systems. Ship false.",
  "affordability_system":   false,
  "governance_layer":       false,
  "event_system":           false,
  "heatmap_overlay":        false,
  "council_checkpoint":     false,
  "json_content_pipeline":  false
}
```

### 3.3 C++ interface (`src/services/FeatureFlags.hxx`)

```cpp
class FeatureFlags : public Singleton<FeatureFlags>
{
public:
  void readFile();                          // call once at startup
  bool affordabilitySystem()  const;
  bool governanceLayer()      const;
  bool eventSystem()          const;
  bool heatmapOverlay()       const;
  bool councilCheckpoint()    const;
  bool jsonContentPipeline()  const;

private:
  FeatureFlags() = default;
  friend Singleton<FeatureFlags>;
  bool m_affordabilitySystem  = false;
  bool m_governanceLayer      = false;
  bool m_eventSystem          = false;
  bool m_heatmapOverlay       = false;
  bool m_councilCheckpoint    = false;
  bool m_jsonContentPipeline  = false;
};
```

Usage:
```cpp
if (FeatureFlags::instance().affordabilitySystem())
{
  m_affordabilityModel.tick(mapNodes);
}
```

---

## 4. Simulation System Designs

### 4.1 CityIndices

**Tick frequency:** every in-game month (hooked into `GameClock`)

**Algorithm:**

For each `MapNode` on the map:
1. Look up its active tile's `TileData`
2. Accumulate weighted per-attribute sums into five buckets
3. Normalise buckets to [0, 100]
4. Cache result and emit `signalIndicesUpdated`

**Normalisation approach:**

Each attribute has a known min/max (defined in `tileData.hxx` as `TD_*_MIN/MAX`
macros). The city-wide index is:

```
index = (sum_of_weighted_values - theoreticalMin) / (theoreticalMax - theoreticalMin) × 100
```

This means indices degrade naturally as density rises without proportional
service coverage.

**Spatial optimisation:**

The full map recompute is O(mapSize²). At 128×128 (16,384 cells) with a fast
inner loop this is <1ms. If map sizes grow, switch to dirty-region tracking
(flag `MapNode` dirty on tile change; only recompute affected cells).

### 4.2 AffordabilityModel

**State (persisted to save file):**

```cpp
struct AffordabilityState
{
  float medianRent;             // derived: zoneDensity × upkeepMultiplier
  float medianIncome;           // derived: inhabitants distribution across LOW/MEDIUM/HIGH
  float affordabilityIndex;     // [0, 100] – primary index
  float displacementPressure;   // [0, 100] – accumulates when affordability < threshold
  float landValueProxy;         // [0, 100] – rises with density and commercial adjacency
};
```

**Tick logic:**

```
1. Compute medianRent from residential zone upkeep density
2. Compute medianIncome from inhabitant density tiers
3. affordabilityIndex = clamp(100 - (rent/income)*50 - displacement*0.3 + policyEffects, 0, 100)
4. If affordabilityIndex < 40: displacementPressure += DISPLACEMENT_RATE
5. Else: displacementPressure = max(0, displacementPressure - RECOVERY_RATE)
6. If displacementPressure > 60: apply population churn to residential nodes
```

**Balance constants** (tunable in `FeatureFlags.json` or a separate `balance.json`):

| Constant | Default | Effect |
|----------|---------|--------|
| `DISPLACEMENT_RATE` | 3.0 | How fast pressure rises per month below threshold |
| `RECOVERY_RATE` | 1.5 | How fast pressure falls when affordability recovers |
| `CHURN_THRESHOLD` | 60 | Pressure level at which population loss begins |
| `MAX_CHURN_PER_TICK` | 0.05 | Max fraction of inhabitants lost per month at max pressure |

### 4.3 PolicyEngine

**Data contract (policy JSON):**

```json
{
  "id":          "string – unique key",
  "label":       "string – display name",
  "description": "string – tooltip",
  "type":        "budget_sink | zone_modifier | service_boost | tax_modifier",
  "cost_per_month": 0,
  "effects": [
    {
      "target": "string – AffordabilityState or CityIndices field name",
      "op":     "add | multiply | set",
      "value":  0.0
    }
  ],
  "ui_slider":   { "min": 0, "max": 1, "step": 1 }
}
```

**C++ engine:**

```
PolicyEngine::tick()
  for each active policy:
    deduct cost_per_month from city budget
    for each effect:
      resolve target field pointer via string → pointer map
      apply op(value)
```

The string → pointer map is built once at load time. This avoids `if`-chains
per policy while remaining data-driven.

### 4.4 EventEngine

**Data contract (event JSON):**

```json
{
  "id": "string",
  "trigger": {
    "index": "affordabilityIndex | safetyIndex | publicTrust | …",
    "op":    "lt | gt | eq",
    "value": 0.0
  },
  "cooldown_months": 6,
  "notification": "Player-facing message string",
  "effects": [ { "target": "…", "op": "…", "value": 0.0 } ]
}
```

**Evaluation order:**

1. Each game-month tick, iterate all loaded events
2. Skip if on cooldown
3. Evaluate trigger condition against current `CityIndices` / `AffordabilityState`
4. If true: apply effects, emit `signalEventFired`, reset cooldown
5. UI subscribes to `signalEventFired` → shows toast notification

### 4.5 GovernanceManager

**Public Trust:**

```cpp
float GovernanceManager::computePublicTrust(const CityIndices &indices) const
{
  return 0.30f * indices.affordability
       + 0.25f * indices.safety
       + 0.20f * indices.jobs
       + 0.15f * indices.commute
       + 0.10f * (100.f - indices.pollution);
}
```

**Trust effects (applied each tick):**

| Trust range | Effect |
|-------------|--------|
| 0–15 | Policy options locked; events queue; `signalCouncilCheckpoint` fires immediately |
| 15–30 | Tax efficiency × 0.80; growth rate × 0.90 |
| 30–70 | Neutral |
| 70–85 | Growth rate × 1.10 |
| 85–100 | Upkeep costs × 0.95; growth rate × 1.20 |

**Council Checkpoint:**

Fires every N in-game months (N from `FeatureFlags.json`, default 12).
Emits `signalCouncilCheckpoint` → UI shows modal → player selects policy pledges.

---

## 5. Data-Driven Content Pipeline

### 5.1 Directory layout

```
data/resources/data/
├── TileData.json               (existing)
├── TerrainGen.json             (existing)
├── UILayout.json               (existing)
├── AudioConfig.json            (existing)
├── FeatureFlags.json           (NEW – Phase 0)
├── policies/
│   ├── affordable_housing_fund.json   (NEW – Phase 1)
│   └── upzoning_incentives.json       (NEW – Phase 1)
└── events/
    ├── rent_strike.json               (NEW – Phase 2)
    ├── business_exodus.json           (NEW – Phase 2)
    └── transit_breakdown.json         (NEW – Phase 2)
```

### 5.2 Loading order at startup

```
1. Settings::readFile()
2. FeatureFlags::readFile()          ← gate everything below
3. TileManager::instance()           (existing)
4. PolicyEngine::loadPolicies()      ← if jsonContentPipeline flag
5. EventEngine::loadEvents()         ← if jsonContentPipeline flag
```

### 5.3 Adding a new policy (contributor workflow)

1. Create `data/resources/data/policies/<id>.json` following the schema in §4.3
2. No C++ changes required for simple `add`/`multiply` effects
3. Add a test in `tests/simulation/PolicyEngine.cxx` asserting the effect applies
4. Enable `json_content_pipeline` in your local `FeatureFlags.json` to test
5. Open PR with label `policy`

---

## 6. Save File Compatibility

New simulation state is added under a versioned key in the existing JSON save format:

```json
{
  "version": 2,
  "mapData": { "…": "…" },
  "simulation": {
    "affordability": { "medianRent": 0, "displacementPressure": 0, "…": 0 },
    "governance":    { "publicTrust": 75, "monthsToCheckpoint": 8 },
    "activePolicies": ["affordable_housing_fund"]
  }
}
```

Older saves (version < 2) load with simulation fields zeroed – city starts
from a neutral baseline. The save version is bumped once per phase.

---

## 7. Testing Strategy

### 7.1 Unit tests for simulation layer

Because `src/simulation/` has **no SDL dependency**, tests compile in pure
C++17 with no display needed. Catch2 is already vendored.

Target test files:
- `tests/simulation/CityIndices.cxx` – index aggregation math
- `tests/simulation/AffordabilityModel.cxx` – rent/income/displacement formulas
- `tests/simulation/PolicyEngine.cxx` – effect application from JSON
- `tests/simulation/EventEngine.cxx` – trigger evaluation and cooldowns

### 7.2 Integration tests

Use existing `tests/game/PowerManager.cxx` as a template:
- Create a minimal map, apply zone tiles, tick the simulation, assert index changes
- These tests require SDL (window creation) but are already in the CI matrix

### 7.3 Balance tests

A headless `tools/balance-sim.cxx` binary (long-term goal) that runs the
simulation for 100 in-game years and dumps index CSV for plotting.
This enables balance tuning without launching the game.

---

## 8. Coding Conventions

These supplement the existing project style.

- **File extensions:** `.hxx`/`.cxx` (existing convention, keep it)
- **Naming:** `CamelCase` for classes, `camelCase` for members, `snake_case` for JSON keys
- **No raw pointers in new code** – use `std::unique_ptr` or references
- **No global state in simulation layer** – pass state explicitly; use `Singleton`
  only in the `services/` layer
- **All balance numbers in data files** – never hardcode a `float` balance value
  in a `.cxx` file; put it in JSON and load it
- **Every public method of a simulation class has a unit test**
- **Feature flag every new system** before merging to `main`

---

## 9. Open Questions / Tech Debt

| Question | Priority | Notes |
|----------|----------|-------|
| Isometric coordinate system limits map size scalability | Low | Current 128×128 is fine; profile before changing |
| `TileManager` is a global singleton accessed everywhere | Medium | Refactor to dependency injection over time |
| AngelScript bindings are incomplete | Low | Expose new simulation APIs to scripts in Phase 2 |
| No spatial index on `mapNodes` | Medium | Needed for efficient proximity queries (commute index) |
| Save format is not versioned yet | High | Add `"version"` field before Phase 1 ships |
| No CI build for Linux (only local) | High | Add GitHub Actions workflow in Phase 0 |
