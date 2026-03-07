# OpenAgora – Gameplay Roadmap

> **What will it feel like to play OpenAgora?**  
> This document describes the planned player experience at each milestone —
> not file paths or class names, but what the player sees, decides, and feels.

For the technical implementation plan see [`ROADMAP.md`](ROADMAP.md).

---

## Vision

OpenAgora is a city builder where **governance is the game**.
You are not just placing tiles; you are making policy choices that carry political
weight, budget consequences, and cascading effects on the people who live in your city.
Every term, residents judge you. Get it wrong and your city fractures. Get it right
and you prove that thoughtful governance can build a city people actually want to live in.

---

## Core Gameplay Loop

```
Build zones  →  City indices change  →  Residents react
      ↑                                       ↓
   Adjust policies  ←  Council checkpoint  ←  Events fire
```

1. **Build** – place residential, commercial, and industrial zones; manage power.
2. **Watch** – five live indices (Affordability, Safety, Jobs, Commute, Pollution) rise and fall.
3. **Respond** – enact, level up, or repeal policies from the policy panel.
4. **Survive** – events fire when indices fall too low; budget tightens when trust collapses.
5. **Face the council** – every six months an approval review tests whether residents still support you.
6. **Adapt** – council feedback constrains your options and forces you to prioritise.

---

## Milestone 0 – Proof of Concept ✅ Playable Now

> *"I can see my city's health and make policies that move the numbers."*

**What the player can do:**
- Place zones and buildings on a procedurally generated isometric map.
- Watch the City Indices Dashboard — five coloured bars with live trend arrows — respond to zone layout.
- Open the Policy Panel and toggle or level up any of the **20+ data-driven policies** across six categories
  (Housing, Economy, Transit, Safety, Environment, Education & Health).
- See the monthly budget balance and the impact each active policy has on spending.
- Receive toast notifications and browse the Event Log when crises trigger (rent strikes,
  business exodus, transit breakdowns, smog alerts, and more).
- Survive Council Checkpoints every six months — a modal review that shows your approval breakdown
  and locks certain policies if trust is too low.

**The five city indices:**

| Index | What it measures |
|-------|-----------------|
| Affordability | Rent-to-income ratio; displacement pressure on residents |
| Safety | Inverted crime and fire-hazard levels |
| Jobs | Employment balance between commercial/industrial demand and residential supply |
| Commute | Road connectivity and zone-distance quality |
| Pollution | Spatial average of industrial and traffic pollution |

**Current limitations:**
- No formal win or loss condition — the game continues even after a lost election (sandbox mode).
- Economy multipliers on zone growth are partially wired; not all policy effects are
  fully reflected in zone expansion speed yet.
- No tutorial; the player must discover the policy-index relationships through experimentation.
- Art and audio are inherited from the Cytopia upstream; original assets have not yet landed.

---

## Milestone 1 – The Living City (v0.5)

> *"My policy choices visibly change how the city grows, not just the index bars."*

**Target gameplay additions:**

### Economy multipliers fully connected to zone growth
- Commercial zone expansion rate scales with the **Jobs index** in real time.
- Residential density growth accelerates or decelerates based on **Affordability**.
- Industrial output directly raises or lowers the **Pollution index** tile by tile.
- Players can experiment with zoning ratios and watch the indices respond predictably.

### Service strain visible in the city
- The **Economy Panel** surfaces transit reliability, education access, and health access
  as named indicators with colour-coded status.
- Hovering a tile shows its local strain contribution in a tooltip.

### Scenario selection at game start
- Five curated starting scenarios available from the main menu:
  - **Housing Crunch** – fast population growth has outpaced supply; displacement pressure is already high.
  - **Industrial Revival** – empty commercial zones and low employment; need to attract businesses.
  - **Fiscal Emergency** – negative starting balance; almost no budget room for new policies.
  - **Balanced Growth** – a comfortable starting point for learning the systems.
  - **Green Transition** – high pollution inherited from heavy industry; environmental policies are unlocked early.
- Each scenario pre-sets starting approval, budget balance, and highlights three recommended policies.

### Difficulty settings
- **Sandbox** – no election loss; all policies always available; unlimited budget.
- **Standard** – normal council approval gates and budget constraints.
- **Challenge** – tighter approval thresholds; stricter budget; events fire more frequently.

---

## Milestone 2 – Politics Matters (v0.6)

> *"Choosing a policy feels like a real political trade-off, not just a number slider."*

### Policy trade-offs with named stakeholder reactions
- Each policy category has associated stakeholder groups (tenants, landlords, business owners,
  transit riders, environmental advocates).
- Enacting a policy logs a short flavour reaction from affected groups in the Event Log.
- Conflicting stakeholder pressure can trigger follow-up events (e.g., a landlord lobby campaign
  after rent control, or a tenant coalition boost after affordable housing funding).

### Unlock tree for advanced policies
- Basic policies are available from the start.
- Advanced policies (e.g., **Climate Resilience Bonds**, **Municipal Broadband**,
  **Anti-Corruption Office**) unlock only after prerequisites are met:
  - A minimum index level sustained for several months, or
  - A lower-tier policy at a minimum level, or
  - A minimum approval rating threshold.
- The Policy Panel shows locked policies with a clear reason (greyed out with tooltip).

### Formal win / loss conditions
- **Election win**: sustain approval above 50 across three consecutive council checkpoints.
- **Soft loss**: approval falls below 15 at a checkpoint — the mayor is voted out, but the sandbox continues with restricted policies.
- **Hard loss** (Challenge difficulty only): budget balance reaches a critical deficit floor — the city enters receivership and the run ends.
- End-of-term summary screen shows key decisions, final index scores, and a resident quote.

### Policy term pledges
- At each council checkpoint, the player commits to 1–2 policy pledges for the next term
  (e.g., "I will hold Affordability above 50" or "I will reduce Pollution by 10 points").
- Delivering on pledges gives an approval bonus; breaking them triggers a trust penalty event.

---

## Milestone 3 – Narrative & Factions (v0.7)

> *"The city feels like it has a political life of its own."*

### Advisor character
- A persistent city advisor provides contextual guidance:
  - Alerts when an index is trending dangerously.
  - Suggests policy options relevant to the current situation.
  - Reacts to player decisions with brief civic-flavoured commentary.
- The advisor has a distinct personality and tone that fits the game's civic theme.

### Factional politics
Three factions with distinct policy preferences:

| Faction | Priorities | Backing bonus | Antagonist trigger |
|---------|-----------|--------------|-------------------|
| **Tenants' Coalition** | Affordability ≥ 55, Safety ≥ 50 | Trust +5 per term | Affordability < 35 |
| **Business Alliance** | Jobs ≥ 55, Commute ≥ 45 | Tax efficiency ×1.05 | Jobs < 30 |
| **Green Council** | Pollution ≤ 35, Commute ≥ 50 | Event cooldown −1 month | Pollution > 65 |

- Each faction has a satisfaction bar visible in the Governance Panel.
- Faction satisfaction feeds into the overall approval rating alongside the index weights.
- Choosing policies that benefit one faction may antagonise another, forcing real trade-offs.

### Expanded event library
- New event categories beyond crises:
  - **Community celebrations** (neighbourhood watch success, festival tourism boost, volunteer cleanup wave).
  - **External shocks** (port strike, healthcare worker strike, flooding season, heatwave).
  - **Political opportunities** (civic innovation grant, startup boom, infrastructure funding offer).
- Choice-based events with multiple outcomes based on current budget and faction satisfaction.

### Campaign scenario chain
A linked sequence of three scenarios that tell an arc:
1. **Inherited Debt** – take over a city in fiscal emergency and stabilise it.
2. **Displacement Crisis** – booming growth has pushed out long-term residents; restore affordability.
3. **Green Reckoning** – industrial pollution has reached a breaking point; pivot to clean growth.

Each scenario carries over a trust score modifier from the previous run.

---

## Milestone 4 – Original IP Launch (v1.0)

> *"OpenAgora is its own game with its own world."*

### Original art and audio
- New isometric tile set designed for the game's civic aesthetic.
- Replacement UI palette and updated icon.
- Original music tracks (commissioned or CC0-sourced).
- Replacement sound effects.

### Lore and world
- The city has a name, a founding era, and a fiction-grounded history.
- Advisor dialogue written to match the game's fictional world.
- Event flavour text references the game's own geography and factions.
- Lore Bible published as `docs/LORE.md`.

### Tutorial
- An interactive tutorial scenario walks the player through:
  1. Zone placement and power connection.
  2. Reading the City Indices Dashboard.
  3. Opening and activating a first policy.
  4. Surviving a first Council Checkpoint.

### Mod support (basic)
- Policy and event JSON files are fully documented as a stable modding API.
- A mod-loading path allows users to drop custom JSON files into a `mods/` folder.
- Custom policies and events appear in-game without recompiling.

---

## Future Vision (Post-1.0)

These features are aspirational and subject to community interest:

- **Regional map** – manage multiple connected cities with shared resource and commute flows.
- **Political parties** – choose a governing platform at game start with distinct starting policies.
- **Community scenario sharing** – upload and download custom scenarios from within the game.
- **Time-lapse replay** – watch your city's index history as an animated chart at the end of a run.
- **Accessibility options** – colourblind palette for heatmap overlays; text scaling; keyboard-only UI.
- **Multiplayer** (long-term) – compete or cooperate across adjacent city maps.

---

## Summary Table

| Milestone | Version | Theme | Key Gameplay Addition |
|-----------|---------|-------|----------------------|
| Proof of Concept | v0.4 ✅ | Policy sandbox | Indices, policies, events, council checkpoints |
| The Living City | v0.5 | Growth wired | Economy multipliers, scenarios, difficulty |
| Politics Matters | v0.6 | Trade-offs | Stakeholder reactions, unlock tree, win/loss |
| Narrative & Factions | v0.7 | Political life | Advisor, factions, campaign chain |
| Original IP Launch | v1.0 | Own identity | Art, audio, lore, tutorial, mod support |
| Regional Expansion | Post-1.0 | Scale up | Regional map, parties, multiplayer |

---

*For technical implementation details behind each milestone, see [`ROADMAP.md`](ROADMAP.md).*  
*For system architecture, see [`DESIGN.md`](DESIGN.md).*
