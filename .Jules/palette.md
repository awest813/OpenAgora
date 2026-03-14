## 2024-03-10 - Adding Contextual Tooltips in Dear ImGui
**Learning:** In native C++ desktop apps using Dear ImGui, standard accessibility principles still apply (e.g., explaining icon-only buttons or destructive actions). We can easily accomplish this by appending `if (ui::IsItemHovered()) ui::SetTooltip("...");` immediately after drawing the UI element.
**Action:** Always check for missing tooltips on icon-only buttons (`ImageButton`) and append clear warnings to destructive actions (like "Quit" or "New Game") to prevent accidental data loss.
## 2023-10-27 - Added disabled tooltip
**Learning:** In Dear ImGui, standard `ui::IsItemHovered()` doesn't fire when an item is disabled via `ui::BeginDisabled()`. You must pass `ImGuiHoveredFlags_AllowWhenDisabled` to `ui::IsItemHovered` to provide context tooltips on disabled buttons.
**Action:** Always check if a button might be disabled (e.g. within a `ui::BeginDisabled()` block), and use the `AllowWhenDisabled` flag for tooltips explaining the disabled state to the user.
