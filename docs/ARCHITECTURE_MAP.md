# OpenAgora – Architecture Map

> Generated as part of the UI Overhaul & Performance Optimization project.

---

## Engine Core

```
main.cxx
  └── Cytopia::Game          (src/Game.{hxx,cxx})
        ├── initialize()     – register SignalMediator callbacks, bootstrap Map
        ├── run()            – main loop (SDL_RenderClear → events → map → UI → present)
        └── quit()           – audio stop, SDL cleanup
```

**Key singletons accessed directly in the loop:**
- `WindowManager` – SDL window + renderer + ImGui frame lifecycle
- `EventManager` – SDL event poll + input dispatch
- `GameClock` – dual real/game-time scheduler (drives monthly sim ticks)
- `UIManager` – ImGui panel orchestrator
- `FrameMetrics` *(new)* – per-frame timing data for the debug overlay

---

## Rendering System

```
WindowManager              (src/engine/WindowManager.{hxx,cxx})
  ├── newImGuiFrame()      – ImGui_ImplSDLRenderer_NewFrame + ImGui::NewFrame
  ├── renderScreen()       – ImGui::Render + SDL_RenderPresent
  └── m_renderer           – SDL_Renderer* (2D hardware accelerated)

Map                        (src/engine/Map.{hxx,cxx})
  ├── renderMap()          – iterate m_visibleNodesCount MapNode pointers
  │     └── MapNode::render() – up to 14 Sprite* layers per node
  └── refresh()            – frustum-cull pMapNodesVisible (MICROPROFILE_SCOPEI)

Sprite                     (src/engine/Sprite.{hxx,cxx})
  └── render()             – SDL_RenderCopy per sprite frame

AffordabilityOverlay       (src/engine/render/AffordabilityOverlay.{hxx,cxx})
  └── render()             – tints residential tile quads with ARGB colour
                             (SDL_BLENDMODE_BLEND) to show affordability heatmap
```

**Render order per frame:**
1. `SDL_RenderClear`
2. `Map::renderMap()` – all visible isometric tile layers
3. `AffordabilityOverlay::render()` – semi-transparent tile tints (if flag on)
4. `UIManager::drawUI()` – ImGui overlay panels
5. `WindowManager::renderScreen()` → `SDL_RenderPresent`

---

## UI Layer

```
UIManager                       (src/engine/UIManager.{hxx,cxx})
  ├── m_menuStack                – modal menus (PauseMenu, SettingsMenu, LoadMenu)
  ├── m_persistentMenu           – always-visible panels (BuildMenu, GameTimeMenu, …)
  ├── drawUI()                   – draws stack back + all persistent panels
  ├── toggleDebugMenu()          – toggles m_showDebugMenu (F3 / F11 / checkbox)
  └── Debug Overlay              – FrameMetrics-driven perf panel (new)

GameMenu base struct            (src/engine/UIManager.hxx)
  └── virtual draw() const

game/ui panels
  ├── BuildMenu                  – tile placement palette
  ├── GameTimeMenu               – pause / 1×/2×/4×/8× speed
  ├── CityIndicesPanel           – five coloured index bars (Affordability … Pollution)
  ├── GovernancePanel            – approval bar, status badges, recent events
  ├── PolicyPanel                – per-policy toggles + budget summary
  ├── NotificationOverlay        – toast popups (auto-dismiss after 8 s)
  ├── PauseMenu                  – pause / resume / settings / quit
  ├── SettingsMenu               – resolution, full-screen, audio, language
  └── LoadMenu                   – save-file browser

UITheme.hxx                     – centralized ImVec4 palette + push/pop helpers
```

---

## Input System

```
EventManager                    (src/engine/EventManager.{hxx,cxx})
  └── checkEvents(SDL_Event&)
        ├── ImGui_ImplSDL2_ProcessEvent()   ← UI gets first pick
        ├── SDL_QUIT                        → SignalMediator::signalQuitGame
        ├── SDL_KEYDOWN
        │     ├── ESC     – toggle pause / cancel tile selection
        │     ├── F3/F11  – toggle debug overlay (new F3 binding)
        │     ├── H       – toggle UI visibility
        │     ├── F       – toggle full-screen
        │     ├── WASD/↑↓←→ – camera pan
        │     ├── 1–8     – toggle map layers
        │     └── CTRL/SHIFT – modify placement/demolish mode
        ├── SDL_MOUSEBUTTONDOWN/UP          → tile place/demolish
        ├── SDL_MOUSEMOTION                 → cursor highlight preview
        └── SDL_MOUSEWHEEL                  → camera zoom
```

Input routing priority: **ImGui > Map tile actions**.  
When ImGui has captured keyboard/mouse (`io.WantCaptureKeyboard` / `WantCaptureMouse`),
tile placement is suppressed.

---

## Game Logic

```
GamePlay                        (src/game/GamePlay.{hxx,cxx})
  └── runMonthlySimulationTick(mapNodes)
        ├── CityIndices::tick()             – aggregate 5 scalar indices (0-100)
        ├── AffordabilityModel::tick()      – rent/income/displacement pressure
        ├── GovernanceSystem::tickMonth()   – approval, events, checkpoint
        ├── BudgetSystem::tick()            – tax revenue, policy expenses
        └── ZoneManager::applyChurn()       – evict residents under pressure

Simulation layer (src/simulation/ – pure C++17, no SDL)
  ├── CityIndices        – weighted aggregation of tile attributes
  ├── AffordabilityModel – rent vs income per zone-area density band
  ├── PolicyEngine       – data-driven policy application (add/multiply/set)
  ├── GovernanceSystem   – approval scoring (weighted average of indices)
  └── BudgetSystem       – monthly revenue and expenditure ledger
```

---

## Asset Management

```
ResourceManager             (src/services/ResourceManager.{hxx,cxx,inl.hxx})
  └── On-demand texture cache (loaded once, reused every frame)

TileManager                 (src/engine/TileManager.{hxx,cxx})
  └── Loads data/resources/data/TileData.json at startup

PolicyEngine                (src/simulation/PolicyEngine.{hxx,cxx})
  └── Scans data/resources/data/policies/*.json at startup (if flag enabled)

GovernanceSystem            (src/simulation/GovernanceSystem.{hxx,cxx})
  └── Scans data/resources/data/events/*.json at startup (if flag enabled)

FeatureFlags                (src/services/FeatureFlags.{hxx,cxx})
  └── Reads data/resources/data/FeatureFlags.json once at startup

Settings                    (src/engine/basics/Settings.{hxx,cxx})
  └── Reads data/resources/settings.json at startup
```

All asset loads are **synchronous on the main thread** at startup.
No lazy background loading is currently implemented (see UI_REFACTOR_PLAN.md).

---

## Architecture Problems Identified

| # | Problem | Location | Severity |
|---|---------|----------|----------|
| 1 | All JSON assets loaded synchronously on the main thread at startup | `Game::run()`, `TileManager`, `PolicyEngine`, `GovernanceSystem` | Medium |
| 2 | No UI dirty-flag – all persistent panels re-render every frame even when data hasn't changed | `UIManager::drawUI()` | Low (ImGui is fast, but CPU cycles wasted) |
| 3 | No explicit input routing layer – ImGui capture check is implicit in SDL event loop | `EventManager::checkEvents()` | Low |
| 4 | `GameStates::drawUI` comment says "temporary until new UI is ready" – never removed | `Game.cxx:243` / `GameStates.hxx` | Low |
| 5 | Frame-rate is uncapped (runs as fast as hardware allows) – wastes GPU power on idle frames | `Game::run()` | Low |
| 6 | Per-frame string concatenation for FPS counter (`"FPS: " + std::to_string(fpsFrames)`) | `Game.cxx:260` | Minor |
| 7 | Map layer toggle keys (1–8) have no UI feedback | `EventManager.cxx` | Minor |
