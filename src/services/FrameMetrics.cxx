#include "FrameMetrics.hxx"

void FrameMetrics::recordFrame(float frameDeltaMs, float uiDeltaMs) noexcept
{
  m_lastFrameMs = frameDeltaMs;
  m_lastUIMs    = uiDeltaMs;

  // Ring-buffer history for the frame-time graph (ImGui PlotLines).
  m_frameHistory[m_historyOffset] = frameDeltaMs;
  m_historyOffset                 = (m_historyOffset + 1) % HISTORY_SIZE;

  // Peak frame time with slow exponential decay so spikes remain visible
  // for ~200 frames before fading.
  m_peakFrameMs = std::max(m_peakFrameMs * 0.995f, frameDeltaMs);

  // Rolling FPS – recalculate once per accumulated second.
  m_fpsAccum += frameDeltaMs;
  m_fpsCount++;
  if (m_fpsAccum >= 1000.f)
  {
    m_avgFPS   = static_cast<float>(m_fpsCount) * 1000.f / m_fpsAccum;
    m_fpsAccum = 0.f;
    m_fpsCount = 0;
  }
}
