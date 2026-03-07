# OpenAgora

**A policy-driven city simulation where governance mechanics *are* the gameplay.**

> Fork of [Cytopia](https://github.com/CytopiaTeam/Cytopia) evolved into a civic city builder.
> Build your city, earn your residents' trust, and navigate the politics of urban policy.

[![GitHub](https://img.shields.io/badge/GitHub-OpenAgora-blue?logo=github)](https://github.com/awest813/OpenAgora)

---

OpenAgora is a free, open-source isometric city-building game built on an SDL2 rendering engine (C++17).
Unlike traditional city builders, the core challenge is **governance**: every policy you enact has costs,
trade-offs, and consequences tracked through a living simulation of affordability, safety, jobs, commute,
and pollution. Residents react. Elections loom. Build a city people actually want to live in.

---

## Current Features

### Engine
- SDL2 isometric rendering engine (C++17)
- Dear ImGui overlay panels
- Camera panning, zooming, and relocating
- Procedural terrain generation (libnoise)
- Zone management (Residential / Commercial / Industrial)
- Power grid simulation

### Simulation Layer
- **City Indices Dashboard** – five live city-wide metrics (affordability, safety, jobs, commute, pollution) with trend arrows
- **Affordability Model** – rent/income ratio, displacement pressure, and population churn
- **Policy Engine 2.0** – leveled policies with prerequisites, exclusivity checks, approval gates, durations, and deep simulation effect targets
- **Governance System** – public trust score, council checkpoints, cooldown events, and choice-based event responses
- **Budget System** – monthly tax revenue with approval multipliers, running balance, 12-month history
- **Economy Depth Model** – unemployment pressure, wage pressure, business confidence, and debt stress
- **Service Strain Model** – transit reliability, safety capacity load, education access stress, and health access stress
- **Event System** – data-driven trigger/effect events with optional player choices and budget-adjusted outcomes
- **Scenario Catalog** – JSON-curated scenario presets with starting conditions and recommended policy packages
- **Affordability Heatmap Overlay** – per-tile green/yellow/red heat tint showing residential density pressure

### UI
- City Indices Panel (sidebar with coloured bars and trend arrows)
- Policy Panel (category-grouped policies with level controls and lock reasons)
- Governance Panel (approval bar, recent event feed, event choices, council checkpoint modal)
- Notification Overlay (toast-style event alerts, 8-second auto-dismiss)
- Event Log Panel (scrollable full notification history)
- Economy Panel (budget + deep economy/service indicators)

---

## Roadmap

See [`docs/ROADMAP.md`](docs/ROADMAP.md) for the full phased plan.

| Phase | Status | Summary |
|-------|--------|---------|
| Phase 0 – Fork hygiene | ✅ Complete | Feature flags, architecture scaffolding, docs |
| Phase 1 – Housing Affordability | ✅ Complete | Full affordability loop + heatmap overlay |
| Phase 2 – Governance Layer | 🔄 Core complete | Public trust, events, council checkpoints |
| Phase 2.5 – Economy Foundation | ✅ Complete | Budget system with approval multipliers |
| Phase 3 – Original IP Conversion | 🔲 Ongoing | New art, audio, lore, and branding |

---

## Supported Platforms

- Linux (clang / g++17 or later)
- Windows
- macOS

---

## Prerequisites

- [CMake 3.16 or later](https://cmake.org/)
- [Conan](https://conan.io)
- [SDL2](https://www.libsdl.org/)
- [SDL2_ttf](https://www.libsdl.org/)
- [SDL2_image](https://www.libsdl.org/)
- [OpenAL](https://www.openal.org/)
- [zlib](https://www.zlib.net/)
- [libnoise](http://libnoise.sourceforge.net/)
- [libogg](https://www.xiph.org/ogg/)
- [libvorbis](https://www.xiph.org/vorbis/)
- [libpng](http://www.libpng.org/pub/png/libpng.html)
- [Dear ImGui](https://github.com/ocornut/imgui)

---

## Build Instructions

```sh
# Clone the repository
git clone https://github.com/awest813/OpenAgora.git
cd OpenAgora

# Install Conan dependencies
pip install conan
./get_dependencies.sh

# Configure and build
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . -j$(nproc)
```

For detailed platform-specific instructions see [`docs/`](docs/).

---

## Running Tests

```sh
cd build
ctest --output-on-failure
# Or run simulation unit tests directly (no display required):
./OpenAgora_Tests "[simulation]"
```

61 simulation-layer unit tests cover city indices, affordability, policy/governance depth,
budget round-tripping, scenario/content loading, and deep economy/service models.

---

## Coding Guidelines

- **File extensions:** `.hxx` / `.cxx` (project convention)
- **Naming:** `CamelCase` classes, `camelCase` members, `snake_case` JSON keys
- **No raw pointers** in new code – use `std::unique_ptr` or references
- **No SDL headers in `src/simulation/`** – the simulation layer must be unit-testable without a display
- **Balance numbers in data files** – never hardcode a float balance value in C++; use JSON
- **Feature-flag every new system** before merging to `main` (see `data/resources/data/FeatureFlags.json`)

See [`docs/DESIGN.md`](docs/DESIGN.md) for full architecture documentation.

---

## Contributing

1. Pick an open issue (look for `good-first-issue` or `phase-1` / `phase-2` labels)
2. Create a branch off `main`
3. Add a unit test in `tests/simulation/` for any new simulation logic
4. Open a PR – CI will run the full test suite

New policies are the easiest contribution: drop a JSON file in
`data/resources/data/policies/` following the schema in [`docs/DESIGN.md §4.3`](docs/DESIGN.md)
and no C++ changes are needed for simple `add`/`multiply` effects.

---

## License

OpenAgora is derived from [Cytopia](https://github.com/CytopiaTeam/Cytopia) which is
licensed under the MIT License. See [`license.txt`](license.txt) for details.
