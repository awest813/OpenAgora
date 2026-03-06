## 2026-03-05 - Adding Tooltips to Underexplained Metrics
**Learning:** The City Indices panel showed labels like 'Affordability' and 'Safety' which are complex aggregate metrics. The icons in the underlying data structure weren't even being rendered. Adding hover tooltips helps explain complex game mechanics to users without cluttering the UI.
**Action:** Look for other areas where complex data structures have unused fields or where game mechanics are summarized into a single word, and add tooltips using `ui::IsItemHovered()` to provide context.

## 2025-01-28 - Adding Tooltips for Destructive Actions
**Learning:** Destructive actions without context (e.g., "Quit Game" or "New Game") can result in accidental data loss and frustrate users. I found that modifying existing ImGui UI lambda abstractions (`centeredButton` in `PauseMenu`) to accept tooltips allows us to quickly inject contextual warnings (e.g., "Abandon current city and start fresh") exactly where they're needed.
**Action:** Always verify if a primary action might result in lost user context or data. If so, immediately add an explanation on hover via `ui::IsItemHovered()` and `ui::SetTooltip()`.
