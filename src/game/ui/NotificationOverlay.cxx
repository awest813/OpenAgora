#include "NotificationOverlay.hxx"
#include "UITheme.hxx"

#include <GovernanceSystem.hxx>

#include "imgui.h"
#include <SDL.h>

#include <cstdio>
#include <string>

namespace ui = ImGui;

// How long each toast is shown (in milliseconds of real time).
static constexpr Uint32 TOAST_DURATION_MS = 8000;

// ── Internal state (persistent across frames) ─────────────────────────────────

namespace
{
  // Index into recentNotifications() of the last notification we started showing.
  static int   s_lastShownIndex = -1;
  // SDL tick when the current toast first appeared.
  static Uint32 s_toastStartMs  = 0;
  // Cached text of the active toast.
  static std::string s_toastText;
  // Month label of the active toast.
  static int   s_toastMonth = 0;
}

// ── draw() ────────────────────────────────────────────────────────────────────

void NotificationOverlay::draw() const
{
  const GovernanceSystem &gov = GovernanceSystem::instance();
  const auto &notes = gov.recentNotifications();

  const int latestIndex = static_cast<int>(notes.size()) - 1;

  // Detect a new notification.
  if (latestIndex > s_lastShownIndex && !notes.empty())
  {
    s_lastShownIndex = latestIndex;
    s_toastStartMs   = SDL_GetTicks();
    s_toastText      = notes.back().text;
    s_toastMonth     = notes.back().month;
  }

  // Check if toast has expired.
  if (s_toastText.empty())
    return;

  const Uint32 elapsed = SDL_GetTicks() - s_toastStartMs;
  if (elapsed >= TOAST_DURATION_MS)
  {
    s_toastText.clear();
    return;
  }

  // Fade out during the last 1500 ms.
  float alpha = 1.f;
  constexpr Uint32 FADE_START_MS = TOAST_DURATION_MS - 1500;
  if (elapsed >= FADE_START_MS)
    alpha = 1.f - static_cast<float>(elapsed - FADE_START_MS) / 1500.f;

  // ── Render ─────────────────────────────────────────────────────────────────

  const ImGuiIO &io = ui::GetIO();
  const float padding = 12.f;
  const float toastWidth = 300.f;

  // Position: bottom-right corner
  const ImVec2 toastPos{io.DisplaySize.x - toastWidth - padding, io.DisplaySize.y - 70.f};

  ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 6.f);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(12.f, 8.f));
  ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.08f, 0.10f, 0.14f, 0.92f * alpha));
  ImGui::PushStyleColor(ImGuiCol_Border,   ImVec4(UITheme::COL_ACCENT.x, UITheme::COL_ACCENT.y, UITheme::COL_ACCENT.z, alpha));
  ImGui::PushStyleColor(ImGuiCol_Text,     ImVec4(UITheme::COL_TEXT_PRIMARY.x, UITheme::COL_TEXT_PRIMARY.y, UITheme::COL_TEXT_PRIMARY.z, alpha));

  ui::SetNextWindowPos(toastPos, ImGuiCond_Always);
  ui::SetNextWindowSize({toastWidth, 0.f}, ImGuiCond_Always);

  const ImGuiWindowFlags flags =
      ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize |
      ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoScrollbar |
      ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoDecoration |
      ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoInputs;

  bool open = true;
  if (ui::Begin("##notification_toast", &open, flags))
  {
    // Month label
    ImGui::PushStyleColor(ImGuiCol_Text,
        ImVec4(UITheme::COL_ACCENT.x, UITheme::COL_ACCENT.y, UITheme::COL_ACCENT.z, alpha));
    ui::Text("M%d  ", s_toastMonth);
    ImGui::PopStyleColor();

    ui::SameLine(0.f, 0.f);
    ImGui::PushStyleColor(ImGuiCol_Text,
        ImVec4(UITheme::COL_TEXT_PRIMARY.x, UITheme::COL_TEXT_PRIMARY.y, UITheme::COL_TEXT_PRIMARY.z, alpha));
    ui::TextWrapped("%s", s_toastText.c_str());
    ImGui::PopStyleColor();
  }
  ui::End();

  ImGui::PopStyleColor(3);
  ImGui::PopStyleVar(2);
}
