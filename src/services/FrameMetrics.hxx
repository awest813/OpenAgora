#pragma once

#include "../util/Singleton.hxx"

#include <array>
#include <algorithm>

/**
 * @brief Lightweight per-frame performance counters.
 *
 * Updated each frame by Game::run() via FrameMetrics::instance().recordFrame().
 * Read by the debug overlay (F3 / F11) and the FPS counter.
 *
 * All time values are in milliseconds.
 */
class FrameMetrics : public Singleton<FrameMetrics>
{
public:
  friend Singleton<FrameMetrics>;

  /// Number of frames retained in the history ring-buffer for the frame-time graph.
  static constexpr int HISTORY_SIZE = 128;

  /**
   * @brief Record one completed frame.
   * @param frameDeltaMs Total frame time in milliseconds (clear → present).
   * @param uiDeltaMs    Time spent in UIManager::drawUI() in milliseconds.
   */
  void recordFrame(float frameDeltaMs, float uiDeltaMs) noexcept;

  /// Total frame time for the most recently completed frame (ms).
  float lastFrameMs() const noexcept { return m_lastFrameMs; }

  /// Time spent inside UIManager::drawUI() for the most recent frame (ms).
  float lastUIMs() const noexcept { return m_lastUIMs; }

  /// Rolling average frames-per-second (updated every second).
  float avgFPS() const noexcept { return m_avgFPS; }

  /// Slowly-decaying peak frame time (ms) – useful to spot intermittent spikes.
  float peakFrameMs() const noexcept { return m_peakFrameMs; }

  /// Ring-buffer of recent frame times for the history graph.
  const std::array<float, HISTORY_SIZE> &frameHistory() const noexcept { return m_frameHistory; }

  /// Current write offset into the ring-buffer (use as ImGui PlotLines overlay_text offset).
  int frameHistoryOffset() const noexcept { return m_historyOffset; }

private:
  FrameMetrics() = default;

  float m_lastFrameMs = 0.f;
  float m_lastUIMs    = 0.f;
  float m_avgFPS      = 0.f;
  float m_peakFrameMs = 0.f;

  // Rolling FPS accumulator (reset every second).
  float m_fpsAccum = 0.f;
  int   m_fpsCount = 0;

  std::array<float, HISTORY_SIZE> m_frameHistory{};
  int m_historyOffset = 0;
};
