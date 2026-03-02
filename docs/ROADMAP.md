# OpenAgora – Modernization Roadmap

> Fork of [Cytopia](https://github.com/CytopiaTeam/Cytopia) being evolved into a
> policy-driven city simulation where governance mechanics *are* the gameplay.

---

## Current State of the Fork

**Engine:** SDL2 + Dear ImGui, C++17, CMake/Conan, libnoise terrain gen  
**Simulation depth today:** Zone management (RCI), power grids, tile placement, terrain generation  
**What's missing:** Economic indices, policy levers, governance feedback loops, population dynamics  

The data-model hooks are already partially there – `TileData` already carries
`inhabitants`, `happiness`, `educationLevel`, `pollutionLevel`, `crimeLevel`,
`fireHazardLevel`, `upkeepCost`. They just aren't aggregated into any city-wide
indices that the player can see or act on.

---

## Phase 0 – Fork Hygiene (1–2 days)

Do these so future-you doesn't hate present-you.

### 0.1 Repository identity
- [ ] Rename repository (e.g. `openagora` or `cytopia-civics`) and update `CMakeLists.txt` project name
- [ ] Update `README.md`: new name, vision statement, build instructions, contribution guide
- [ ] Add `/docs/ROADMAP.md` (this file) and `/docs/DESIGN.md`

### 0.2 GitHub project hygiene
- [ ] Create issue labels: `policy`, `simulation`, `ui`, `content`, `refactor`, `good-first-issue`, `phase-1`, `phase-2`, `phase-3`
- [ ] Create a GitHub Project board mirroring the phases below
- [ ] Add 10 seed issues (see bottom of this document)

### 0.3 Feature flags
- [ ] Add `data/resources/data/FeatureFlags.json` – JSON key/value booleans toggling in-development systems
- [ ] Add `src/services/FeatureFlags.hxx/.cxx` – singleton that reads the file and exposes typed getters
- [ ] Gate every new Phase-1+ system behind a flag (`affordability_system`, `governance_layer`, etc.)

### 0.4 Architecture scaffolding
- [ ] Create `src/simulation/` directory (will hold pure-logic simulation modules)
- [ ] Create `data/resources/data/policies/` directory (will hold JSON policy definitions)
- [ ] Create `data/resources/data/events/` directory (will hold JSON event definitions)

---

## Phase 1 – Flagship Policy System: Housing Affordability (2–3 weeks)

### Why this system?
It touches map data (zone density, land value proxies), the economy (upkeep/budget),
population (inhabitants field), and UI (overlay + sliders). Shipping it proves the
full stack works end-to-end.

### 1.1 City Indices Dashboard

**New file:** `src/simulation/CityIndices.hxx/.cxx`

Aggregate per-tile `TileData` fields into five city-wide scalar indices (0–100):

| Index | Source fields | Notes |
|-------|--------------|-------|
| Affordability | inhabitants, zoneDensity, upkeepCost | Median rent proxy |
| Safety | crimeLevel, fireHazardLevel | Inverted, higher = safer |
| Jobs/Economy | inhabitants (commercial/industrial), upkeepCost | Employment pressure |
| Commute | road connectivity, zone distance | Graph metric |
| Pollution | pollutionLevel, zone proximity | Spatial average |

- Computed on a `GameClock` tick (every in-game month)
- Exposed via `SignalMediator` signals so UI can subscribe without coupling
- Persisted in save file under `"cityIndices"` key

**UI:** `src/game/ui/CityIndicesPanel.hxx/.cxx` – ImGui sidebar showing five coloured bars + trend arrows

### 1.2 Affordability Index + Rent/Income Model

**New file:** `src/simulation/AffordabilityModel.hxx/.cxx`

```
affordabilityIndex = clamp(
    100 - (medianRent / medianIncome) * 50
    - displacementPressure * 0.3
    + affordableHousingFundEffect
    + upzoningEffect,
  0, 100
)
```

State tracked per model tick:
- `medianRent` – derived from land value proxy (zone density × upkeep multiplier)
- `medianIncome` – abstracted across three wealth tiers (LOW/MEDIUM/HIGH density)
- `affordabilityIndex` – [0–100], feeds into CityIndices
- `displacementPressure` – [0–100], accumulates when affordabilityIndex < threshold

### 1.3 Displacement Pressure + Population Churn

When `displacementPressure > 60`:
- Residential zones begin "churning" (inhabitant count decreases per tick)
- `publicTrust` index drops (feeds Phase 2)
- In-game notification fires: *"Rising rents are pushing residents out."*

Churn rate formula:
```
churnRate = (displacementPressure - 60) / 40 * maxChurnPerTick
```

### 1.4 Policy: Affordable Housing Fund

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

C++ side: `PolicyEngine` reads definitions, applies effects each game-month tick.

### 1.5 Policy: Upzoning Incentives

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

### 1.6 Heatmap Overlay: Affordability

- Reuse existing `MapLayers` bitmask system to add an `AFFORDABILITY_OVERLAY` layer
- Each `MapNode` gets a `float affordabilityScore` computed from its zone + neighbours
- Rendered as a colour gradient (green → yellow → red) blended over the tile texture
- Toggled from the existing layer visibility UI

### Phase 1 Milestone
Ship a devlog: *"OpenAgora adds an affordability simulation loop."*
Player can place zones, watch affordability degrade as density rises without intervention,
then apply sliders to stabilise it.

---

## Phase 2 – Governance Layer (3–6 weeks)

### 2.1 Public Trust Metric

`publicTrust` [0–100] – the master approval number.

Computed as a weighted average of all CityIndices:

```
publicTrust = 0.30 × affordability
            + 0.25 × safety
            + 0.20 × jobs
            + 0.15 × commute
            + 0.10 × (100 - pollution)
```

Effects of `publicTrust`:
- < 30: Tax efficiency penalty (−20%), growth bonus inverted to penalty
- < 15: Events trigger (see 2.3), policy options constrained
- > 70: Growth bonus (+10%), reduced upkeep costs

### 2.2 Council Checkpoint (Approval Gate)

Every X in-game months (configurable in `FeatureFlags.json`):
- Snapshot current `publicTrust`
- Display a **Council Review Panel** (modal ImGui window):
  - "Your approval rating is: 62 / 100"
  - Breakdown by index with pass/fail badges
  - Option to commit to 1–2 policy pledges for next term
- If trust < 30: **soft fail** – sandbox continues but policy options locked for 3 months, event fires

### 2.3 Event System

**Data-driven** (`data/resources/data/events/`):

```json
{
  "id": "rent_strike",
  "trigger": { "index": "affordabilityIndex", "op": "lt", "value": 30 },
  "cooldown_months": 6,
  "notification": "Tenants have gone on rent strike in the warehouse district.",
  "effects": [
    { "target": "taxEfficiency", "op": "multiply", "value": 0.8 },
    { "target": "publicTrust",   "op": "add",      "value": -8  }
  ]
}
```

`EventEngine` (`src/simulation/EventEngine.hxx/.cxx`):
- Evaluates trigger conditions each game-month tick
- Fires notifications via `SignalMediator::signalEvent`
- Tracks per-event cooldowns to prevent spam

### 2.4 Notification UI

- Toast-style ImGui overlay (bottom-right, auto-dismiss after 8s)
- Event log panel (accessible from toolbar, lists last 20 events with timestamps)

### Phase 2 Milestone
The sim now has a **theme**, not just mechanics. The player manages a city *and* a
political relationship with its residents.

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
│   ├── CityIndices.hxx/.cxx
│   ├── AffordabilityModel.hxx/.cxx
│   ├── PolicyEngine.hxx/.cxx
│   └── EventEngine.hxx/.cxx
├── game/           ← existing zone/power managers + new governance
│   ├── GovernanceManager.hxx/.cxx
│   └── ui/         ← ImGui panels
├── engine/         ← map, rendering, input (existing)
├── render/         ← heatmap overlays, sprite compositing (new)
└── services/       ← clock, audio, feature flags (existing + new)

data/resources/data/
├── policies/       ← JSON policy definitions
├── events/         ← JSON event trigger+effect definitions
└── FeatureFlags.json
```

**Principle:** New simulation systems must compile and be unit-testable without SDL.
Policy and event definitions live in JSON. No hardcoded balance numbers in C++.

---

## Next 10 GitHub Issues

| # | Title | Labels |
|---|-------|--------|
| 1 | Add City Indices dashboard (affordability, safety, jobs, commute, pollution) | `simulation`, `ui`, `phase-1` |
| 2 | Implement affordability index + rent/income model | `simulation`, `phase-1` |
| 3 | Displacement pressure + population churn effects | `simulation`, `phase-1` |
| 4 | Policy: Affordable Housing Fund (budget → reduces displacement) | `policy`, `phase-1` |
| 5 | Policy: Upzoning Incentives (density growth modifier) | `policy`, `phase-1` |
| 6 | Heatmap overlay: affordability | `ui`, `phase-1` |
| 7 | Event system: trigger + effect + notification UI | `simulation`, `ui`, `phase-2` |
| 8 | Public Trust metric + effects on growth/tax efficiency | `simulation`, `phase-2` |
| 9 | Council checkpoint: approval gate every X months | `simulation`, `ui`, `phase-2` |
| 10 | Content pipeline: policies/events defined in JSON/YAML | `content`, `refactor`, `phase-1` |

---

## Definition of Done (per phase)

**Phase 0:** Docs committed, labels created, feature flag system compiles and is tested.  
**Phase 1:** Affordability loop visible to player; heatmap toggleable; policies have measurable effect; devlog written.  
**Phase 2:** Council checkpoint fires; events trigger from index thresholds; publicTrust affects gameplay; soft-fail scenario playable.  
**Phase 3:** Binary and UI strings renamed; at least one original art asset in place; no Cytopia branding visible to player.
