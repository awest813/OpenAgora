# OpenAgora – Modernization Roadmap

> Fork of [Cytopia](https://github.com/CytopiaTeam/Cytopia) being evolved into a
> policy-driven city simulation where governance mechanics *are* the gameplay.

This document tracks **technical implementation phases**.  
For the **player-facing feature plan** see [`GAMEPLAY_ROADMAP.md`](GAMEPLAY_ROADMAP.md).

---

## Current State of the Fork

**Engine:** SDL2 + Dear ImGui, C++17, CMake/Conan, libnoise terrain gen  
**Simulation depth today:** Zone management (RCI), power grids, tile placement, terrain generation,
city-wide indices, affordability model, policy engine, governance loop with council checkpoints,
event system, budget system, economy depth model, service strain model, zone-type growth multipliers,
affordability heatmap overlay, scenario selection at game start, and three-level difficulty system  
**What's still in progress:** Phase 3 IP conversion (original art, audio, lore)

The data-model hooks are already partially there – `TileData` already carries
`inhabitants`, `happiness`, `educationLevel`, `pollutionLevel`, `crimeLevel`,
`fireHazardLevel`, `upkeepCost`. They are now aggregated into city-wide indices
that the player can see, and the policy/event systems act on them.

---

## Phase 0 – Fork Hygiene ✅ Complete

### 0.1 Repository identity
- [x] Rename repository to `openagora` and update `CMakeLists.txt` project name
- [x] Update `README.md`: new name, vision statement, build instructions, contribution guide
- [x] Add `/docs/ROADMAP.md` (this file) and `/docs/DESIGN.md`

### 0.2 GitHub project hygiene
- [ ] Create issue labels: `policy`, `simulation`, `ui`, `content`, `refactor`, `good-first-issue`, `phase-1`, `phase-2`, `phase-3`
- [ ] Create a GitHub Project board mirroring the phases below
- [ ] Add 10 seed issues (see bottom of this document)

### 0.3 Feature flags ✅
- [x] Add `data/resources/data/FeatureFlags.json` – JSON key/value booleans toggling in-development systems
- [x] Add `src/services/FeatureFlags.hxx/.cxx` – singleton that reads the file and exposes typed getters
- [x] Gate every new Phase-1+ system behind a flag (`affordability_system`, `governance_layer`, etc.)

### 0.4 Architecture scaffolding ✅
- [x] Create `src/simulation/` directory (will hold pure-logic simulation modules)
- [x] Create `data/resources/data/policies/` directory (will hold JSON policy definitions)
- [x] Create `data/resources/data/events/` directory (will hold JSON event definitions)

---

## Phase 1 – Flagship Policy System: Housing Affordability ✅ Complete

### Why this system?
It touches map data (zone density, land value proxies), the economy (upkeep/budget),
population (inhabitants field), and UI (overlay + sliders). Shipping it proves the
full stack works end-to-end.

### 1.1 City Indices Dashboard ✅

**File:** `src/simulation/CityIndices.hxx/.cxx`

Aggregates per-tile `TileData` fields into five city-wide scalar indices (0–100):

| Index | Source fields | Notes |
|-------|--------------|-------|
| Affordability | inhabitants, zoneDensity, upkeepCost | Median rent proxy |
| Safety | crimeLevel, fireHazardLevel | Inverted, higher = safer |
| Jobs/Economy | inhabitants (commercial/industrial), upkeepCost | Employment pressure |
| Commute | road connectivity, zone distance | Graph metric |
| Pollution | pollutionLevel, zone proximity | Spatial average |

- Computed on a `GameClock` tick (every 30 in-game days ≈ one month)
- `overrideAffordability()` lets `AffordabilityModel` replace the density-only heuristic with its full model output
- Trend arrows shown in UI by comparing `current()` vs `previous()` snapshots

**UI:** `src/game/ui/CityIndicesPanel.hxx/.cxx` – ImGui sidebar showing five coloured bars + trend arrows ✅

**Tests:** `tests/simulation/CityIndices.cxx` – 6 unit tests covering all five indices ✅

### 1.2 Affordability Index + Rent/Income Model ✅

**File:** `src/simulation/AffordabilityModel.hxx/.cxx`

```
affordabilityIndex = clamp(
    100 - (medianRent / medianIncome) * 50
    - displacementPressure * 0.3
    + policyAffordabilityBonus,
  0, 100
)
```

State tracked per model tick (`AffordabilityState`):
- `medianRent` – derived from land value proxy (zone density × upkeep multiplier)
- `medianIncome` – three wealth tiers (LOW=30, MEDIUM=50, HIGH=60) plus employment surplus bonus (+up to 20)
- `affordabilityIndex` – [0–100], overrides the density-only heuristic in CityIndices each tick
- `displacementPressure` – [0–100], accumulates when affordabilityIndex < threshold
- `landValueProxy` – [0–100], rises with commercial density and high-density residential

All balance constants are configurable via `AffordabilityModel::Config`:

| Constant | Default | Effect |
|----------|---------|--------|
| `affordabilityThreshold` | 40.0 | Index below which displacement accelerates |
| `displacementRate` | 3.0 | Pressure increase per month below threshold |
| `recoveryRate` | 1.5 | Pressure decrease per month above threshold |
| `churnThreshold` | 60.0 | Pressure level at which population loss begins |
| `maxChurnPerTick` | 0.05 | Max fraction of inhabitants lost per month at max pressure |

**Tests:** `tests/simulation/AffordabilityModel.cxx` – 7 unit tests ✅

### 1.3 Displacement Pressure + Population Churn ✅

When `displacementPressure > churnThreshold (default 60)`:
- `AffordabilityModel::populationChurnActive()` returns `true`
- `AffordabilityModel::churnRate()` returns the fractional inhabitant loss per month

Churn rate formula:
```
churnRate = (displacementPressure - churnThreshold) / (100 - churnThreshold) * maxChurnPerTick
```

Applied each month in `GamePlay::runMonthlySimulationTick()` via `ZoneManager::applyChurn(churnFraction)`,
which marks a fraction of occupied residential zone nodes as vacant so buildings can respawn at lower density.

### 1.4 Policy: Affordable Housing Fund ✅

**Data-driven definition** (`data/resources/data/policies/affordable_housing_fund.json`):

```json
{
  "id": "affordable_housing_fund",
  "label": "Affordable Housing Fund",
  "description": "Diverts budget to subsidise below-market housing.",
  "type": "budget_sink",
  "cost_per_month": 500,
  "effects": [
    { "target": "displacementPressure", "op": "add", "value": -5 },
    { "target": "affordabilityIndex",   "op": "add", "value": 8  }
  ],
  "ui_slider": { "min": 0, "max": 2000, "step": 100 }
}
```

### 1.5 Policy: Upzoning Incentives ✅

```json
{
  "id": "upzoning_incentives",
  "label": "Upzoning Incentives",
  "description": "Allows higher-density residential development.",
  "type": "zone_modifier",
  "effects": [
    { "target": "zoneDensityGrowthRate", "op": "multiply", "value": 1.4 },
    { "target": "landValueProxy",        "op": "multiply", "value": 1.15 }
  ],
  "ui_slider": { "min": 0, "max": 1, "step": 1 }
}
```

### 1.6 Policy Engine ✅

**File:** `src/simulation/PolicyEngine.hxx/.cxx`

- Loads all `*.json` files from `data/resources/data/policies/` at startup (when `json_content_pipeline = true`)
- `setActive(id, bool)` toggles policies at runtime
- `tick(AffordabilityState, CityIndicesData)` applies all active policy effects each month, returns total monthly budget cost
- Supports `add`, `multiply`, `set` ops on all `AffordabilityState` fields and all five `CityIndicesData` fields
- Effect dispatch via normalized lowercase string matching – no hardcoded `if`-chains in `.cxx`

**Tests:** `tests/simulation/PolicyEngine.cxx` – 9 unit tests covering active/inactive, add/multiply/set ops, clamping, and cost accumulation ✅

**UI integration:** PolicyPanel with per-policy toggles + cost display ✅ (see `src/game/ui/PolicyPanel.hxx/.cxx`)

### 1.7 Heatmap Overlay: Affordability ✅

**File:** `src/engine/render/AffordabilityOverlay.hxx/.cxx`

- Each visible residential `Sprite` gets a semi-transparent colour tint based on its zone density:
  - LOW density → green (affordable)
  - MEDIUM density → yellow (moderate pressure)
  - HIGH density → red (unaffordable)
- Rendered via SDL `SDL_RenderFillRect` with `SDL_BLENDMODE_BLEND` after the main tile layer
- Hooked into `Map::renderMap()` – called after all tile sprites so the tint sits on top of buildings
- Enabled by the `heatmap_overlay` feature flag in `FeatureFlags.json` (now `true`)

### Phase 1 Milestone ✅
Ship a devlog: *"OpenAgora adds an affordability simulation loop."*
Player can place zones, watch affordability degrade as density rises without intervention,
then apply sliders to stabilise it. Affordability heatmap shows density pressure per tile.

---

## Phase 2 – Governance Layer ✅ Core complete

### 2.1 Public Trust Metric ✅

`publicTrust` [0–100] – the master approval number.

Computed as a weighted average of all CityIndices (DESIGN.md §4.5):

```
publicTrust = 0.30 × affordability
            + 0.25 × safety
            + 0.20 × jobs
            + 0.15 × commute
            + 0.10 × (100 - pollution)
```

Note: pollution is inverted (high pollution → low approval contribution).

Gameplay effects of `publicTrust` (planned):
- < 30: Tax efficiency penalty (−20%), growth bonus inverted to penalty
- < 15: Events trigger (see 2.3), policy options constrained
- > 70: Growth bonus (+10%), reduced upkeep costs

**Status:** approval score computed by `GovernanceSystem::tickMonth()` and displayed in `GovernancePanel`;
gameplay multipliers now wired into growth and budget/tax behavior, including event-driven tax efficiency modifiers.

### 2.2 Council Checkpoint (Approval Gate) ✅

Every N in-game months (default 6, configurable via `FeatureFlags.json`):
- Snapshot current `publicTrust`
- Display a **Council Review Panel** (modal ImGui window):
  - "Your approval rating is: 62 / 100"
  - Breakdown by all five indices
  - Option to commit to 1–2 policy pledges for next term
- If approval < `constraintThreshold` (default 40): policy options constrained, lock persists `policyLockMonths` (default 3)
- If approval < `softFailThreshold` (default 15): **soft fail** – sandbox continues, `lostElection = true`

### 2.3 Event System ✅

**Data-driven** (`data/resources/data/events/`):

Ten data-driven events currently bundled (including positive, negative, and choice-based events):

| Event | Trigger | Effects | Cooldown |
|-------|---------|---------|----------|
| `rent_strike` | affordabilityIndex < 30 | publicTrust −8, taxEfficiency ×0.80 | 6 months |
| `business_exodus` | jobsIndex < 25 | publicTrust −10, taxEfficiency ×0.90, medianIncome ×0.95 | 12 months |
| `transit_breakdown` | commuteIndex < 20 | commuteIndex −15, publicTrust −6, pollutionIndex +8 | 9 months |

`GovernanceSystem` evaluates all loaded events each month tick, respects per-event cooldowns, supports global distress
threshold gating, and can queue player-choice event decisions with budget effects.
Events evaluate against all six trigger targets: `affordabilityIndex`, `safetyIndex`, `jobsIndex`,
`commuteIndex`, `pollutionIndex`, `publicTrust`.

### 2.4 Notification UI ✅

- Recent events list in GovernancePanel sidebar (last 4 events with month stamps) ✅
- Full log of up to 20 events in `GovernanceSystem::recentNotifications()` ✅
- Toast-style overlay (bottom-right, auto-dismiss after 8s) ✅ `src/game/ui/NotificationOverlay.hxx/.cxx`
- Full event log panel from toolbar ✅ `src/game/ui/EventLogPanel.hxx/.cxx`

### Phase 2 Milestone
The sim now has a **theme**, not just mechanics. The player manages a city *and* a
political relationship with its residents.

---

## Phase 2.5 – Economy Foundation

### 2.5 Budget System ✅

**File:** `src/simulation/BudgetSystem.hxx/.cxx`

Tracks monthly city income and expenditure:

- **Tax revenue** = `totalInhabitants × taxRatePerInhabitant × approvalMultiplier`
  - High approval (>70): ×1.10 revenue bonus
  - Low approval (<30): ×0.80 revenue penalty
- **Policy expenses** = sum of `cost_per_month` for all active policies (from PolicyEngine)
- **Running balance** accumulates across months
- Rolling 12-month history for trend display

Integrated into `GamePlay::runMonthlySimulationTick()` behind the `budget_system` feature flag.

**Tests:** `tests/simulation/BudgetSystem.cxx` – 6 unit tests covering reset, tax scaling, approval multipliers, expenses, accumulation, and history rollover ✅

**UI integration:** BudgetSystem balance shown in PolicyPanel header ✅

### 2.5.1 Economy Depth Model ✅

**File:** `src/simulation/EconomyDepthModel.hxx/.cxx`

Tracks four deeper economic state variables updated every game-month:

| Field | Description |
|-------|-------------|
| `unemploymentPressure` | [0–100] – higher = more unemployment |
| `wagePressure` | [0–100] – higher = stronger labour demand |
| `businessConfidence` | [0–100] – higher = stronger investment appetite |
| `debtStress` | [0–100] – higher = greater fiscal strain |

These are stored in `SimulationContextData` and exposed to all UI panels.

**Feature flag:** `economy_depth_model: true` (now enabled) ✅

**Tests:** `tests/simulation/EconomyDepthModel.cxx` – 3 unit tests ✅

### 2.5.2 Service Strain Model ✅

**File:** `src/simulation/ServiceStrainModel.hxx/.cxx`

Tracks four public-service reliability/stress metrics every game-month:

| Field | Description |
|-------|-------------|
| `transitReliability` | [0–100] – higher = better service |
| `safetyCapacityLoad` | [0–100] – higher = more overloaded |
| `educationAccessStress` | [0–100] – higher = poorer access |
| `healthAccessStress` | [0–100] – higher = poorer access |

These are stored in `SimulationContextData` and displayed in EconomyPanel.

**Feature flag:** `service_strain_model: true` (now enabled) ✅

**Tests:** `tests/simulation/ServiceStrainModel.cxx` – 3 unit tests ✅

### 2.5.3 Economy Multipliers → Zone Growth ✅

**Files:** `src/simulation/ZoneGrowthFormulas.hxx`, `src/game/ZoneManager.hxx/.cxx`, `src/game/GamePlay.cxx`

Per-zone-type growth multipliers now applied every game-month so that the player's
economic policies visibly change *how* their city develops – not just the indices:

| Zone Type | Driver | Boost condition | Penalty condition |
|-----------|--------|-----------------|-------------------|
| **Residential** | `affordabilityIndex` | ≥ 65 → ×1.15 | < 30 → ×0.80 |
| **Commercial** | `businessConfidence` + `unemploymentPressure` | confidence ≥ 65 → ×1.15 | confidence < 35 → ×0.85; unemployment > 70 → −10% |
| **Industrial** | `pollutionIndex` + `debtStress` | pollution < 30 → ×1.10 | pollution > 70 → ×0.80; debt > 60 → −15% |

Fallback: when `economy_depth_model` is disabled, commercial growth uses `jobsIndex` directly.

The multiplier logic lives in a pure inline function `computeZoneGrowthMultipliers()` in
`ZoneGrowthFormulas.hxx` so it can be unit-tested without SDL.

`ZoneManager::setZoneTypeGrowthMultipliers()` stores per-type values that are combined
with the global governance multiplier in `spawnBuildings()`.

**Tests:** `tests/simulation/ZoneGrowthFormulas.cxx` – 10 unit tests ✅

### 2.5.4 Economy Panel + Service Strain UI ✅

**File:** `src/game/ui/EconomyPanel.cxx`

The Economy Panel now displays all eight economy-depth and service-strain metrics with:

- **Colour-coded text** – green/yellow/red based on each metric's health direction
- **Tooltips on hover** – explains what each value means and how to improve it
- **Organised sections** – "Economy Depth" and "Service Strain" with section headers
- **Balance colour** – green for positive, red for negative

**Feature flags:** `economy_panel: true`, `event_log_panel: true` (both now enabled) ✅

### Phase 2.5 Milestone ✅
Player can now see that their policies change *how their city grows*: affordable
cities expand faster residentially, confident economies spawn more commercial
buildings, and polluted or debt-ridden cities see their industrial sector slow down.
All eight economy/service metrics are visible with contextual colour coding and tooltips.

---

## Phase 2.6 – Scenario Selection + Difficulty ✅ Complete

### 2.6.1 Scenario Selection at Game Start ✅

**Files:** `src/game/ui/NewGameScreen.hxx/.cxx`

A modal screen shown after "New Game" is clicked, before the city loads:

- Lists all five bundled scenarios with label, description, starting approval, balance, and recommended policies.
- Player selects a scenario card; the selected scenario is applied on "Start Game".
- Screen follows the same result-enum pattern as `LoadMenu` for clean integration with `MainMenu.cxx`.

**Integration:** `src/MainMenu.cxx` – "New Game" now shows `NewGameScreen` instead of immediately starting ✅

`ScenarioCatalog` stores the pending selection (scenario ID + difficulty) that `Game::newGame()` consumes.

### 2.6.2 Difficulty Settings ✅

**File:** `src/simulation/DifficultySettings.hxx`

Three difficulty presets, selectable in `NewGameScreen` alongside the scenario:

| Difficulty | Council Checkpoints | Soft-Fail Threshold | Policy Lock | Checkpoint Interval |
|------------|---------------------|---------------------|-------------|---------------------|
| **Sandbox** | Disabled | 0 (never) | 0 months | — |
| **Standard** | Enabled | 15 | 3 months | Every 6 months |
| **Challenge** | Enabled | 25 | 4 months | Every 4 months |

`DifficultyConfig` is a plain struct; `difficultyConfig(DifficultyLevel)` returns the preset values.

`Game::applyConfiguredScenario()` reads the pending difficulty and re-configures `GovernanceSystem` so
each new game honours the player's selection.

**Tests:** `tests/simulation/DifficultySettings.cxx` – 8 unit tests ✅

### Phase 2.6 Milestone ✅ — v0.5 "The Living City" Complete

Player can now:
1. Choose one of five curated scenarios at game start, each with distinct starting conditions.
2. Select Sandbox / Standard / Challenge difficulty to tailor the governance pressure.
3. Watch their economic policies visibly change how the city grows (residentially, commercially, industrially).
4. See all eight economy-depth and service-strain metrics with colour-coded status.

---

## Phase 2.7 – Governance & Policy Depth ✅ Core complete

### 2.7.1 Policy Term Pledges ✅

**Files:** `src/simulation/GovernanceSystem.hxx/.cxx`, `src/game/ui/GovernancePanel.cxx`

At each council checkpoint the player may commit to one measurable term pledge:

- **Dynamic pledge options** – `availablePledges()` returns 4 options scaled to the current
  city state. If Affordability is struggling (< 60), the pledge asks to hold it above 40;
  if healthy it asks to sustain it above 55. Same adaptive logic for Safety, Jobs, and Pollution.
- **Pledge evaluation** – at the *next* checkpoint `evaluatePledge()` checks whether the
  condition was met. A kept pledge applies `+bonusApproval`; a broken one applies `−penaltyApproval`.
  Both trigger a notification in the event log.
- **Persisted state** – `GovernancePersistedState` now carries `hasPledge`, `activePledge`,
  `consecutiveSuccessfulCheckpoints`, and `wonElection` so the pledge survives save/load.
- **UI** – pledge selection is shown in the Council Checkpoint modal; the active pledge
  is shown in the Governance Panel sidebar.

**Tests:** 6 new unit tests in `tests/simulation/GovernanceSystem.cxx` ✅

### 2.7.2 Formal Election Win Condition ✅

**Files:** `src/simulation/GovernanceSystem.hxx/.cxx`, `src/game/ui/GovernancePanel.cxx`

- `GovernanceSystem` tracks `m_consecutiveSuccessfulCheckpoints` (incremented each checkpoint
  where approval stays above the soft-fail threshold, reset to 0 on election loss).
- After `WIN_CONSECUTIVE_CHECKPOINTS` (3) consecutive wins: `m_wonElection = true`.
- UI: "★ Term Secured – three-term majority" banner in the Governance Panel and a
  "★ Electoral victory secured" banner in the checkpoint modal.
- Consecutive win streak counter is shown in the panel: "Consecutive wins: N / 3".

### 2.7.3 Stakeholder Reactions on Policy Activation ✅

**Files:** `src/simulation/PolicyEngine.hxx/.cxx`, `src/game/ui/PolicyPanel.cxx`,
`data/resources/data/policies/*.json`

When the player activates or deactivates a policy that carries reaction text, a civic
notification is pushed to the governance event log.

**Data fields (JSON → C++ struct):**

| JSON key | C++ field | Description |
|----------|-----------|-------------|
| `stakeholder_reaction_on`  | `PolicyDefinition::stakeholderReactionOn`  | Flavour text on activation |
| `stakeholder_reaction_off` | `PolicyDefinition::stakeholderReactionOff` | Flavour text on deactivation |

**Policies with reactions now authored:** Affordable Housing Fund, Rent Stabilization, Upzoning
Incentives, Congestion Pricing, Transit Priority Lanes, Community Policing, Clean Air Act,
Small Business Microloans (8 of 20 policies). Remaining policies use empty strings (silent toggle).

**Implementation:** `GovernanceSystem::pushStakeholderReaction()` is a public helper called
by `PolicyPanel::draw()` when level transitions cross the inactive/active boundary.

**Tests:** `PolicyEngineV2` – stakeholderReaction fields survive `addDefinition` round-trip ✅

### Phase 2.7 Milestone ✅
Player governance decisions now carry visible political weight:
- Council checkpoints reward consistency through consecutive-win tracking and an electoral win flag.
- Term pledges add a meaningful goal for each council term with real approval consequences.
- Activating contentious policies (rent control, clean air act, congestion pricing) logs
  named stakeholder reactions that enrich the event log and reward civic storytelling.

---

## Phase 2.8 – Non-Policy Depth & Polish (planned)

See `GAMEPLAY_ROADMAP.md §Milestone 2.7` for the full feature descriptions.

| Feature | File(s) | Notes |
|---------|---------|-------|
| Hard loss (receivership) | `GovernanceSystem`, `GamePlay`, new `ReceivershipScreen` | Challenge difficulty only |
| End-of-term summary screen | `src/game/ui/EndOfTermScreen.hxx/.cxx` | Win and soft-loss triggered |
| Tile hover tooltip (local strain) | `Map.cxx`, overlay system | Shows inhabitants, density, pollution |
| Zone growth visual feedback | `ZoneArea.cxx`, `Signal<TileGrowthEvent>` | Green/red pulse on density change |
| Index trend sparklines | `CityIndices.hxx` (5-slot ring buffer), `CityIndicesPanel.cxx` | `ImGui::PlotLines` |
| Policy impact preview tooltip | `PolicyPanel.cxx` | Shows Δ-values before activation |

---

## Phase 3 – Original IP Conversion (Ongoing / D-track)

This phase runs in parallel with Phase 2 once a stable base exists. It is mostly
content, branding, and UX voice work.

### 3.1 Namespace + Binary Rename
- Rename CMake project, binary name, and all namespaces incrementally
- Do NOT rename in a single commit – merge small PRs per subsystem to avoid breaking builds
- Order: CMakeLists → directory names → class names → UI strings → save-file format

### 3.2 Visual Identity
- [ ] New title screen and main menu art
- [ ] Replacement UI palette (avoid Cytopia's palette exactly)
- [ ] Placeholder tile set (even 16×16 recolours count as a milestone)
- [ ] New app icon

### 3.3 Audio
- [ ] Commission or CC0-source replacement music tracks
- [ ] Replace SFX library

### 3.4 Content Voice
- [ ] City names drawn from fictional geography (not real cities)
- [ ] Advisor character with distinct personality and policy-flavoured dialogue
- [ ] Event flavour text written to match the game's civic theme

### 3.5 Lore Bible (`/docs/LORE.md`)
- City name, founding era, factions (landlords vs. tenants vs. business)
- Political parties with distinct policy preferences
- Disaster/event mythology grounded in the game's world

---

## Architecture Target

The goal is to cleanly separate concerns so any subsystem can be replaced or tested
in isolation. See `DESIGN.md` for the detailed module breakdown.

```
src/
├── simulation/     ← pure logic, tick-based, deterministic, no SDL
│   ├── CityIndices.hxx/.cxx           ✅ (29 simulation tests pass)
│   ├── AffordabilityModel.hxx/.cxx    ✅
│   ├── GovernanceSystem.hxx/.cxx      ✅ (pledges, win condition, stakeholder reactions)
│   ├── PolicyEngine.hxx/.cxx          ✅ (levels/prereqs/exclusivity/stakeholder reactions)
│   ├── BudgetSystem.hxx/.cxx          ✅ (persistence hooks)
│   ├── EconomyDepthModel.hxx/.cxx     ✅
│   ├── ServiceStrainModel.hxx/.cxx    ✅
│   ├── SimulationContext.hxx/.cxx     ✅
│   ├── ZoneGrowthFormulas.hxx         ✅ (pure zone-type multiplier logic, testable)
│   ├── ScenarioCatalog.hxx/.cxx       ✅
│   └── DifficultySettings.hxx         ✅ (header-only; Sandbox/Standard/Challenge presets)
├── game/           ← existing zone/power managers + new governance
│   ├── GamePlay.hxx/.cxx              ✅ (monthly sim tick + churn + zone-type growth wiring)
│   └── ui/         ← ImGui panels
│       ├── CityIndicesPanel.hxx/.cxx  ✅
│       ├── GovernancePanel.hxx/.cxx   ✅ (pledge selection, win banner, streak counter)
│       ├── PolicyPanel.hxx/.cxx       ✅ (leveled controls + availability + stakeholder reactions)
│       ├── NotificationOverlay.hxx/.cxx ✅
│       ├── EventLogPanel.hxx/.cxx     ✅
│       ├── EconomyPanel.hxx/.cxx      ✅
│       └── NewGameScreen.hxx/.cxx     ✅ (scenario + difficulty selection at game start)
├── engine/         ← map, rendering, input (existing)
│   └── render/
│       └── AffordabilityOverlay.hxx/.cxx ✅ (heatmap overlay)
├── render/         ← heatmap overlays, sprite compositing
└── services/       ← clock, audio, feature flags
    └── FeatureFlags.hxx/.cxx          ✅

data/resources/data/
├── policies/       ← JSON policy definitions  ✅ (20 policies, 8 with stakeholder_reaction fields)
├── events/         ← JSON event trigger+effect definitions  ✅ (20 events, incl. choice events + compound triggers)
├── scenarios/      ← JSON scenario presets  ✅ (5 scenarios)
└── FeatureFlags.json  ✅ (all Phase 1+2 flags now enabled)
```

**Principle:** New simulation systems must compile and be unit-testable without SDL.
Policy and event definitions live in JSON. No hardcoded balance numbers in C++.

---

## Next 10 GitHub Issues

| # | Title | Labels |
|---|-------|--------|
| 1 | ~~Add City Indices dashboard (affordability, safety, jobs, commute, pollution)~~ ✅ | `simulation`, `ui`, `phase-1` |
| 2 | ~~Implement affordability index + rent/income model~~ ✅ | `simulation`, `phase-1` |
| 3 | ~~Displacement pressure → apply population churn to map `MapNode` inhabitants~~ ✅ | `simulation`, `phase-1` |
| 4 | ~~Policy: Affordable Housing Fund (budget → reduces displacement)~~ ✅ | `policy`, `phase-1` |
| 5 | ~~Policy: Upzoning Incentives (density growth modifier)~~ ✅ | `policy`, `phase-1` |
| 6 | ~~Heatmap overlay: affordability~~ ✅ | `ui`, `phase-1` |
| 7 | ~~PolicyPanel UI: per-policy toggles + monthly cost display~~ ✅ | `ui`, `phase-1` |
| 8 | ~~Event system: trigger + effect + notification UI~~ ✅ | `simulation`, `ui`, `phase-2` |
| 9 | ~~Economy system: budget tracking with approval multipliers~~ ✅ | `simulation`, `phase-2` |
| 10 | ~~Toast notification overlay~~ ✅ / ~~Full event log panel~~ ✅ | `ui`, `phase-2` |

---

## Definition of Done (per phase)

**Phase 0:** Docs committed, labels created, feature flag system compiles and is tested. ✅  
**Phase 1:** Affordability loop visible to player; heatmap toggleable; policies have measurable effect; devlog written. ✅  
**Phase 2:** Council checkpoint fires ✅; events trigger from index thresholds ✅; publicTrust affects gameplay ✅; soft-fail scenario playable ✅.  
**Phase 3:** Binary and UI strings renamed; at least one original art asset in place; no Cytopia branding visible to player.
