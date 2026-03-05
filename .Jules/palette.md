## 2026-03-05 - Adding Tooltips to Underexplained Metrics
**Learning:** The City Indices panel showed labels like 'Affordability' and 'Safety' which are complex aggregate metrics. The icons in the underlying data structure weren't even being rendered. Adding hover tooltips helps explain complex game mechanics to users without cluttering the UI.
**Action:** Look for other areas where complex data structures have unused fields or where game mechanics are summarized into a single word, and add tooltips using `ui::IsItemHovered()` to provide context.
