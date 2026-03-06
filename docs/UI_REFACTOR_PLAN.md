# OpenAgora – UI Refactor Plan

> Companion to ARCHITECTURE_MAP.md and PERFORMANCE_REPORT.md.  
> This document records the target UI architecture and tracks incremental
> refactor steps.  It is updated as work progresses.

---

## Current State

The UI is built on **Dear ImGui** rendered through the SDL2 renderer backend.
Panels are `GameMenu` subclasses stored in two lists inside `UIManager`:

- `m_menuStack` – modal menus (only the top-of-stack is drawn)
- `m_persistentMenu` – always-visible panels drawn every frame

All panels are data-driven at the *content* level (they read from simulation
singletons) but the layout is hard-coded as pixel offsets in each `draw()`
method.

---

## Target Architecture

```
src/engine/ui/
  UIManager          ← orchestrator (already exists)
  UIStateMachine     ← maps GameState → which panels are active (NEW)

src/game/ui/
  BuildMenu          ← tile palette (exists)
  GameTimeMenu       ← speed controls (exists)
  CityIndicesPanel   ← 5 index bars (exists)
  GovernancePanel    ← approval + events (exists)
  PolicyPanel        ← policy toggles (exists)
  NotificationOverlay← toast popups (exists)
  PauseMenu          ← pause/quit modal (exists)
  SettingsMenu       ← settings modal (exists)
  LoadMenu           ← save-file browser (exists)
  DebugOverlay       ← perf overlay (implemented as inline in UIManager.cxx)

src/game/ui/
  UITheme.hxx        ← centralized palette + helpers (exists)
```

---

## Refactor Phases

### Phase 1 – Runtime performance instrumentation ✅ DONE

**What was done:**
- Added `FrameMetrics` singleton (`src/services/FrameMetrics.hxx/.cxx`).
- `Game::run()` measures total frame time and UI render time each frame via
  `SDL_GetPerformanceCounter()`.
- `UIManager::drawUI()` draws a performance debug overlay when
  `m_showDebugMenu` is true.
- Added **F3** as an additional toggle key (F11 already existed).

**What was slow:** No sub-frame timing existed.  Only FPS was tracked (once per
second, integer resolution).

**Why it was slow:** `SDL_GetTicks()` was used only for the coarse FPS update.
`SDL_GetPerformanceCounter()` was never called.

**How the fix helps:** Developers can now see frame time and UI cost in
real time from the in-game overlay without attaching an external profiler.

---

### Phase 2 – UIStateMachine (planned)

**Goal:** Replace the implicit `m_menuStack + m_persistentMenu` pattern with a
formal state machine that maps named states to panel sets.

**Proposed states:**

```cpp
enum class UIState
{
  None,        // no map loaded – blank screen
  MainMenu,    // title / new-game / load-game screen
  InGame,      // normal gameplay (all persistent panels visible)
  Paused,      // PauseMenu on top of InGame panels
  Settings,    // SettingsMenu on top
  Loading,     // asset load screen
};
```

**Proposed API:**

```cpp
class UIStateMachine
{
public:
  void transitionTo(UIState next);
  UIState current() const;
};
```

`UIManager` holds an `UIStateMachine` and uses it to decide which panels to
draw, replacing the current ad-hoc `m_menuStack.back()->draw()` call.

**Why this matters:** Currently the "active state" has to be inferred from
`m_menuStack.size()` and which persistent menus were added.  A state machine
makes transitions explicit and testable.

---

### Phase 3 – UI Dirty Flag (planned)

**Goal:** Avoid rebuilding every panel's ImGui draw list on every frame.

**Proposed approach:**

```cpp
// UIManager
bool m_uiDirty = true;

// After each simulation tick (GovernanceSystem, CityIndices, etc.)
UIManager::instance().markDirty();

// In drawUI()
if (!m_uiDirty) { /* re-submit last draw list only */ return; }
// ... draw panels ...
m_uiDirty = false;
```

**Caveat:** Dear ImGui requires `NewFrame()` / `Render()` every frame regardless.
The dirty-flag optimization applies only to the panel-specific CPU work (reading
simulation state, computing progress bar values, etc.).  For the current panel
count this saving is minor, but it becomes significant if the UI grows to dozens
of panels or if simulation queries become expensive.

---

### Phase 4 – Async Asset Loading (planned)

**Goal:** Move JSON parsing and texture loading off the main thread.

**Components:**

```
AssetManager  – dispatches load jobs to a background thread pool
AssetCache    – thread-safe map of assetID → loaded resource
Preloader     – loads a prioritized list of assets before the main loop starts
```

**Loading order:**
1. Critical (blocking): `settings.json`, `FeatureFlags.json` – must be done before
   anything else
2. Background: `TileData.json`, `policies/*.json`, `events/*.json` – can load
   while the title screen is displayed
3. Lazy: tile textures – load on first reference from `ResourceManager`

**Why this matters:** On slow storage (HDD, network drive), startup can stall
for 1–3 seconds.  Async loading eliminates this freeze.

---

### Phase 5 – Input Router (planned)

**Goal:** Make the ImGui ↔ game input split explicit and testable.

**Proposed design:**

```cpp
class InputRouter
{
public:
  /// Returns true if the UI has captured keyboard input this frame.
  static bool uiHasKeyboard();
  /// Returns true if the UI has captured mouse input this frame.
  static bool uiHasMouse();
};
```

`EventManager::checkEvents()` calls `InputRouter::uiHasKeyboard()` /
`uiHasMouse()` before dispatching game-side events.  Currently ImGui's `io`
flags are checked directly in scattered `if` statements.

---

### Phase 6 – Frame Rate Cap / VSync (planned)

**Goal:** Eliminate wasted CPU/GPU cycles during idle frames.

**Options (in preference order):**

1. Enable SDL vsync: `SDL_CreateRenderer(…, SDL_RENDERER_PRESENTVSYNC)` in
   `WindowManager`.  Zero code in the game loop; driver controls the frame rate.
2. Manual cap: measure elapsed time after `renderScreen()`; sleep for the
   remaining budget using `SDL_Delay`.

**Recommendation:** Use vsync with a user-configurable toggle in `settings.json`.

---

### Phase 7 – UI Virtualization (future, if needed)

For `LoadMenu` save-file lists and any future inventory/server-browser lists,
render only visible rows:

```cpp
// Instead of:
for (const auto &save : allSaves)
    renderRow(save);

// Use:
ImGuiListClipper clipper;
clipper.Begin(static_cast<int>(allSaves.size()));
while (clipper.Step())
    for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; ++i)
        renderRow(allSaves[i]);
```

`ImGuiListClipper` is already bundled with the vendored Dear ImGui.

---

## Key Principles

- **UI must be data-driven**: panel content comes from simulation singletons, not
  hard-coded strings.
- **UI must be event-driven**: panels react to signals (e.g.,
  `GovernanceSystem::signalEventFired`) rather than polling every frame for
  state changes.
- **UI must be independent of game logic**: no `Map` or `TileManager` calls
  inside panel `draw()` methods (currently mostly satisfied).
- **UI should only re-render when dirty**: simulation data changes at most once
  per 30-second game month; panels should not perform expensive queries 60 times
  per second.
- **Never break gameplay systems**: all refactors are incremental and behind
  feature flags where possible.
