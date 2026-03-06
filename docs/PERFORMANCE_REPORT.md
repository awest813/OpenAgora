# OpenAgora – Performance Report

> Produced as part of the UI Overhaul & Performance Optimization project.  
> Analysis is based on static code inspection and runtime instrumentation added
> in this update (see `src/services/FrameMetrics.hxx`).

---

## Instrumentation Added

`FrameMetrics` (`src/services/FrameMetrics.hxx/.cxx`) is a new lightweight
singleton updated every frame from `Game::run()`:

```cpp
// Game.cxx – per-frame measurements
const Uint64 frameStart = SDL_GetPerformanceCounter();
// ... events, map render ...
const Uint64 uiStart = SDL_GetPerformanceCounter();
WindowManager::instance().newImGuiFrame();
uiManager.drawUI();
const float uiMs   = (SDL_GetPerformanceCounter() - uiStart)  * 1000.f / perfFreq;
WindowManager::instance().renderScreen();
const float frameMs = (SDL_GetPerformanceCounter() - frameStart) * 1000.f / perfFreq;
FrameMetrics::instance().recordFrame(frameMs, uiMs);
```

All measurements are in **milliseconds**.  
The debug overlay (F3 or F11) displays them in real time.

Pre-existing MicroProfile scopes remain active when built with
`-DUSE_MICROPROFILE=ON`:
- `"Map" / "Gameloop"` – entire frame
- `"Map" / "Render Map"` – `Map::renderMap()`
- `"Map" / "Refresh Map"` – frustum culling
- `"EventManager" / "checkEvents"` – input polling
- `"UI" / "draw UI"` – `UIManager::drawUI()`

---

## Estimated Performance Budget (60 FPS target = 16.7 ms / frame)

| Phase | System | Estimated cost | Notes |
|-------|--------|---------------|-------|
| 1 | SDL_RenderClear | < 0.1 ms | trivial |
| 2 | EventManager::checkEvents | < 0.5 ms | SDL_PollEvent + ImGui event forwarding |
| 3 | GameClock::tick | < 0.1 ms | timer evaluation only |
| 4 | Map::renderMap (small map, ~32×32) | 1–4 ms | SDL_RenderCopy per visible node × layers |
| 5 | AffordabilityOverlay::render | 0–1 ms | only if `heatmapOverlay` flag enabled |
| 6 | UIManager::drawUI (all panels) | 0.5–2 ms | ImGui CPU + SDL renderer draw data |
| 7 | WindowManager::renderScreen | 1–5 ms | ImGui GPU flush + SDL_RenderPresent (vsync) |
| **Total** | | **~6–12 ms** | healthy margin for 60 FPS |

---

## Identified Bottlenecks

### B1 – Synchronous startup asset loading

**What is slow:** `TileManager`, `PolicyEngine`, `GovernanceSystem`, and
`FeatureFlags` all read and parse JSON files synchronously on the main thread
during `Game::run()` initialization (before the loop starts).  
On slow storage or large data sets this can cause a visible "freeze" at startup.

**Why it is slow:** `std::ifstream` + `nlohmann::json::parse` blocks the calling
thread. With `std::filesystem::directory_iterator` over multiple policy files,
the cost compounds.

**Fix:** Load assets in a background thread (see `UI_REFACTOR_PLAN.md § Async
Asset Loading`).  
**Estimated improvement:** Startup time reduction of 0.5–2 s on typical hardware.

---

### B2 – Uncapped frame rate

**What is slow:** The game loop runs without any frame-rate cap.  
On a fast GPU the renderer can spin at 500–2000 FPS during idle frames (e.g.,
when the map is small and the player is not interacting), consuming 100 % of one
CPU core and burning GPU cycles unnecessarily.

**Why it is slow:** There is no `SDL_Delay`, vsync, or frame-limiter in the loop.

**Fix:** Enable vsync (`SDL_RENDERER_PRESENTVSYNC`) in `WindowManager`, or add a
manual limiter that sleeps for the remainder of the 16.7 ms budget.  
**Estimated improvement:** CPU usage drops from ~100 % to < 5 % at idle; GPU
temperature and power draw both drop significantly.

---

### B3 – Per-frame string allocation in the FPS counter

**What is slow:** Every second, `"FPS: " + std::to_string(fpsFrames)` constructs
a temporary `std::string` on the heap.

**Why it is slow:** Heap allocation for a short string that is the same width
every frame is unnecessary.

**Fix:** Use `std::snprintf` into a stack-local `char[]` buffer, or use
`std::format` (C++20).  This has been noted but not yet changed because the
per-second frequency makes the real-world impact negligible.

---

### B4 – Redundant per-frame panel redraws

**What is slow:** All persistent UI panels (`CityIndicesPanel`,
`GovernancePanel`, `PolicyPanel`, etc.) read their simulation state and rebuild
their ImGui draw-list every frame, even when no simulation tick has occurred.

**Why it is slow:** The monthly tick runs once every 30 in-game days, but the
panels query `CityIndices::instance().current()` on every rendered frame.

**Fix:** A UI dirty-flag system: set `uiDirty = true` after each simulation tick;
panels skip rebuilding their draw lists when `!uiDirty`.  Because Dear ImGui
re-submits draw commands every frame regardless, the actual saving is in the CPU
side (query + branch elimination per panel).  Impact is minor on a modern CPU
but becomes relevant when the number of panels grows.

---

## Debug Overlay

The new developer overlay (toggle **F3** or **F11**, or the "debug" checkbox in
the FPS counter) displays:

```
⚙ Performance Debug Overlay
────────────────────────────
FPS         61.0
Frame       16.3 ms       ← green < 16.7 ms
UI           1.9 ms
Peak        22.4 ms       ← yellow: recent spike
────────────────────────────
[▂▃▂▂▃▂▂▂▂▂▃▂▂▃▂▂▂▂▂▂] frame-time graph (128 frames)
Menus open: 0
```

All values are live – no restart required.

---

## Before / After (Estimated)

| Metric | Before (baseline) | After this PR |
|--------|-------------------|---------------|
| Frame visibility | FPS counter only (integer, 1 s update) | Live frame-time graph + sub-ms breakdown |
| UI timing | Not measured | `FrameMetrics::lastUIMs()` updated every frame |
| Peak spike detection | None | Slow-decay peak in overlay |
| Debug toggle | F11 only (empty panel) | F3 + F11, populated perf overlay |
| Startup timing | Not measured | Groundwork laid (FrameMetrics available) |
