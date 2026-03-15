## 2024-03-10 - Adding Contextual Tooltips in Dear ImGui
**Learning:** In native C++ desktop apps using Dear ImGui, standard accessibility principles still apply (e.g., explaining icon-only buttons or destructive actions). We can easily accomplish this by appending `if (ui::IsItemHovered()) ui::SetTooltip("...");` immediately after drawing the UI element.
**Action:** Always check for missing tooltips on icon-only buttons (`ImageButton`) and append clear warnings to destructive actions (like "Quit" or "New Game") to prevent accidental data loss.

## 2024-03-24 - Disabled Button Tooltips
**Learning:** When a button is disabled using `ui::BeginDisabled()` in Dear ImGui, users still need to know why. It is important to display tooltips on disabled interactive UI elements.
**Action:** Use `ui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)` right after `ui::EndDisabled()` to properly detect hover state and apply tooltips to greyed-out elements like buttons.
