#pragma once

#include "imgui.h"

/**
 * @brief Centralized palette and style helpers for all game UI panels.
 *
 * All panels should call UITheme::pushPanelStyle() / popPanelStyle() to get a
 * consistent look, and use the colour constants when setting individual widget
 * colours.
 */
namespace UITheme
{

// ── Colour palette ────────────────────────────────────────────────────────────

// Panel backgrounds
inline constexpr ImVec4 COL_BG_DARK   {0.08f, 0.09f, 0.12f, 0.93f};
inline constexpr ImVec4 COL_BG_MID    {0.12f, 0.14f, 0.18f, 0.95f};
inline constexpr ImVec4 COL_BG_LIGHT  {0.18f, 0.21f, 0.28f, 1.00f};

// Accent / highlight
inline constexpr ImVec4 COL_ACCENT         {0.30f, 0.65f, 1.00f, 1.00f}; // sky-blue
inline constexpr ImVec4 COL_ACCENT_HOVER   {0.45f, 0.75f, 1.00f, 1.00f};
inline constexpr ImVec4 COL_ACCENT_ACTIVE  {0.20f, 0.55f, 0.90f, 1.00f};

// Text
inline constexpr ImVec4 COL_TEXT_PRIMARY   {0.92f, 0.93f, 0.95f, 1.00f};
inline constexpr ImVec4 COL_TEXT_SECONDARY {0.60f, 0.65f, 0.72f, 1.00f};
inline constexpr ImVec4 COL_TEXT_DISABLED  {0.38f, 0.40f, 0.45f, 1.00f};
inline constexpr ImVec4 COL_HEADER_TEXT    {0.75f, 0.85f, 1.00f, 1.00f};

// Status colours
inline constexpr ImVec4 COL_GREEN  {0.24f, 0.82f, 0.40f, 1.00f};
inline constexpr ImVec4 COL_YELLOW {0.97f, 0.80f, 0.20f, 1.00f};
inline constexpr ImVec4 COL_RED    {0.95f, 0.28f, 0.22f, 1.00f};
inline constexpr ImVec4 COL_ORANGE {0.97f, 0.58f, 0.15f, 1.00f};

// Border / separator
inline constexpr ImVec4 COL_BORDER    {0.25f, 0.30f, 0.40f, 0.80f};
inline constexpr ImVec4 COL_SEPARATOR {0.20f, 0.24f, 0.32f, 1.00f};

// Button colours
inline constexpr ImVec4 COL_BTN       {0.18f, 0.22f, 0.32f, 1.00f};
inline constexpr ImVec4 COL_BTN_HOVER {0.26f, 0.34f, 0.52f, 1.00f};
inline constexpr ImVec4 COL_BTN_ACTV  {0.15f, 0.45f, 0.80f, 1.00f};

// ── Status helpers ─────────────────────────────────────────────────────────────

inline ImVec4 statusColor(float value)
{
  if (value >= 66.f) return COL_GREEN;
  if (value >= 33.f) return COL_YELLOW;
  return COL_RED;
}

inline const char *trendArrow(float cur, float prev)
{
  const float delta = cur - prev;
  if (delta >  2.f) return " ^";
  if (delta < -2.f) return " v";
  return " -";
}

// ── Style push/pop helpers ────────────────────────────────────────────────────

// Number of style vars pushed by pushPanelStyle / pushButtonStyle.
// Must match the corresponding pop call.
inline constexpr int PANEL_STYLE_VAR_COUNT  = 4;
inline constexpr int PANEL_STYLE_COL_COUNT  = 6;
inline constexpr int BTN_STYLE_VAR_COUNT    = 2;
inline constexpr int BTN_STYLE_COL_COUNT    = 3;

/** Push shared panel-level ImGui style vars + colours. */
inline void pushPanelStyle()
{
  ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding,  6.0f);
  ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding,   4.0f);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,   ImVec2(12.f, 10.f));
  ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,     ImVec2(8.f,  6.f));

  ImGui::PushStyleColor(ImGuiCol_WindowBg,           COL_BG_DARK);
  ImGui::PushStyleColor(ImGuiCol_Border,             COL_BORDER);
  ImGui::PushStyleColor(ImGuiCol_Text,               COL_TEXT_PRIMARY);
  ImGui::PushStyleColor(ImGuiCol_TextDisabled,       COL_TEXT_SECONDARY);
  ImGui::PushStyleColor(ImGuiCol_Separator,          COL_SEPARATOR);
  ImGui::PushStyleColor(ImGuiCol_SeparatorActive,    COL_ACCENT);
}

inline void popPanelStyle()
{
  ImGui::PopStyleColor(PANEL_STYLE_COL_COUNT);
  ImGui::PopStyleVar(PANEL_STYLE_VAR_COUNT);
}

/** Push button-specific styles on top of panel style. */
inline void pushButtonStyle()
{
  ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(10.f, 6.f));
  ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 5.0f);

  ImGui::PushStyleColor(ImGuiCol_Button,        COL_BTN);
  ImGui::PushStyleColor(ImGuiCol_ButtonHovered, COL_BTN_HOVER);
  ImGui::PushStyleColor(ImGuiCol_ButtonActive,  COL_BTN_ACTV);
}

inline void popButtonStyle()
{
  ImGui::PopStyleColor(BTN_STYLE_COL_COUNT);
  ImGui::PopStyleVar(BTN_STYLE_VAR_COUNT);
}

// ── Section header helper ─────────────────────────────────────────────────────

/** Draws a coloured header label followed by a separator. */
inline void sectionHeader(const char *label)
{
  ImGui::PushStyleColor(ImGuiCol_Text, COL_HEADER_TEXT);
  ImGui::TextUnformatted(label);
  ImGui::PopStyleColor();
  ImGui::Separator();
}

} // namespace UITheme
