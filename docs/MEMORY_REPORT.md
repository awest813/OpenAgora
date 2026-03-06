# OpenAgora – Memory Report

> Produced as part of the UI Overhaul & Performance Optimization project.

---

## Summary

OpenAgora's memory profile is healthy for a desktop city-builder.  
No per-frame heap allocations were found in the hot render path.
The main allocation concerns are at **startup** (asset loading) and in
**simulation tick callbacks** (vectors rebuilt monthly).

---

## Allocation Hotspots

### H1 – Startup: JSON parsing (one-time, acceptable)

**Location:** `TileManager::init()`, `PolicyEngine::loadPolicies()`,
`GovernanceSystem::loadEventDefinitions()`

**Pattern:** `nlohmann::json::parse(std::ifstream(...))` allocates a parse tree
per file.  After parsing, the tree is consumed into typed structs and the JSON
object is destroyed.

**Impact:** One-time at startup, typically < 10 MB total.  
**Action:** No change needed unless startup memory becomes a constraint.

---

### H2 – Monthly simulation tick: temporary vectors

**Location:** `GamePlay::runMonthlySimulationTick()` (src/game/GamePlay.cxx)

**Pattern:**
```cpp
std::vector<const TileData *> buildingTiles;
for (const auto &node : mapNodes)
    if (auto *tile = node.getTileData(Layer::BUILDINGS))
        buildingTiles.push_back(tile);
```

A `std::vector` is allocated and populated on the heap every month.  
For a 128 × 128 map (16 384 nodes) with ~30 % coverage, this is ~5 000 pointer
pushes per tick.

**Impact:** One heap allocation of ~40 KB per month (≈ once every 30 game days).
This is negligible in isolation; however, if more subsystems adopt the same
pattern the monthly GC spike could grow.

**Recommendation:** Pre-allocate a reusable `m_buildingTilesCache` member in
`GamePlay` and call `.clear()` + `.reserve()` at the start of each tick to
reuse the existing allocation.

---

### H3 – Per-second FPS string allocation

**Location:** `Game::run()` (src/Game.cxx)

**Pattern:**
```cpp
uiManager.setFPSCounterText("FPS: " + std::to_string(fpsFrames));
```

`std::to_string` and `operator+` both allocate on the heap.  Occurs once per
second.

**Impact:** Negligible.  
**Recommendation:** Replace with `std::snprintf` into a stack buffer if the
codebase ever migrates to an allocator-instrumented mode.

---

### H4 – ImGui per-frame draw list

**Location:** `UIManager::drawUI()` (src/engine/UIManager.cxx)

Dear ImGui allocates and rebuilds its draw list on every call to
`ImGui::NewFrame()` / `ImGui::Render()`.  On a typical frame with 6 panels this
is ~5–15 KB of vertex/index buffer data built on the ImGui internal allocator
(not the system heap by default).

**Impact:** Within normal ImGui usage; no action required.  
**Long-term note:** If the UI grows to dozens of panels, consider splitting into
separate ImGui viewports or using ImGui's retain-mode draw list approach.

---

### H5 – NotificationOverlay toast queue

**Location:** `GovernanceSystem::recentNotifications()` →
`NotificationOverlay::draw()` (src/game/ui/NotificationOverlay.cxx)

A `std::deque<std::string>` of recent event strings is maintained in
`GovernanceSystem`.  Each `std::string` allocates on the heap when a governance
event fires.

**Impact:** Low frequency (at most a few events per month); total queue depth is
capped at 4 entries.  No action required.

---

## Memory Budget (Estimated)

| Category | Estimated size | Notes |
|----------|---------------|-------|
| Tile sprite textures (SDL) | 20–80 MB | Depends on number of tiles loaded |
| Map node array (128×128) | ~8 MB | `MapNode` structs with 14 layer pointers |
| JSON parse trees (startup) | < 10 MB | Freed after parse |
| ImGui draw list (per frame) | < 1 MB | Managed by ImGui allocator |
| Simulation state singletons | < 1 MB | CityIndices, GovernanceSystem, BudgetSystem, … |
| Audio buffers (OGG) | 2–10 MB | Depends on `AudioMixer` streaming buffer size |
| **Total estimate** | **~35–100 MB** | Well within typical desktop constraints |

---

## Recommendations

| Priority | Action |
|----------|--------|
| Medium | Pre-allocate `GamePlay::m_buildingTilesCache` to avoid monthly vector reallocation |
| Low | Replace `"FPS: " + std::to_string(fpsFrames)` with `snprintf` into a stack buffer |
| Low | Consider an object pool for `GovernanceSystem` event strings if event frequency increases |
| Future | Instrument with ASAN and Valgrind/heaptrack on a dedicated profiling build to verify these estimates before optimization |
