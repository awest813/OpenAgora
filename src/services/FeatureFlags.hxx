#ifndef FEATURE_FLAGS_HXX_
#define FEATURE_FLAGS_HXX_

#include "../util/Singleton.hxx"

/**
 * @class FeatureFlags
 * @brief Reads FeatureFlags.json and gates in-development systems at runtime.
 *
 * Any system added behind a flag can be merged to main without affecting
 * players. Set the flag to true only in your local FeatureFlags.json to test.
 *
 * Usage:
 * @code
 *   if (FeatureFlags::instance().affordabilitySystem())
 *   {
 *     m_affordabilityModel.tick(mapNodes);
 *   }
 * @endcode
 */
class FeatureFlags : public Singleton<FeatureFlags>
{
public:
  friend Singleton<FeatureFlags>;

  /// Load flags from the bundled FeatureFlags.json. Called once at startup.
  void readFile();

  // ── Phase 1 flags ──────────────────────────────────────────────────────────

  /// Affordability index, rent/income model, and displacement pressure system.
  bool affordabilitySystem() const { return m_affordabilitySystem; }

  /// Heatmap overlay rendered per-tile for affordability score.
  bool heatmapOverlay() const { return m_heatmapOverlay; }

  /// Data-driven policy loading from data/resources/data/policies/*.json.
  bool jsonContentPipeline() const { return m_jsonContentPipeline; }

  // ── Phase 2 flags ──────────────────────────────────────────────────────────

  /// Public trust metric and its effects on tax efficiency / growth.
  bool governanceLayer() const { return m_governanceLayer; }

  /// Event trigger+effect+notification system loaded from events/*.json.
  bool eventSystem() const { return m_eventSystem; }

  /// Monthly council checkpoint modal with approval rating and policy pledges.
  bool councilCheckpoint() const { return m_councilCheckpoint; }

  /// City indices dashboard panel (affordability, safety, jobs, commute, pollution).
  bool cityIndicesDashboard() const { return m_cityIndicesDashboard; }

  /// Governance checkpoint interval in in-game months.
  int governanceCheckpointMonths() const { return m_governanceCheckpointMonths; }

  /// Approval threshold below which policy options are constrained.
  float governanceConstraintThreshold() const { return m_governanceConstraintThreshold; }

  /// Approval threshold below which civic events are evaluated.
  float governanceEventThreshold() const { return m_governanceEventThreshold; }

  /// Approval threshold below which the player loses re-election (soft fail).
  float governanceSoftFailThreshold() const { return m_governanceSoftFailThreshold; }

  /// Number of months policy constraints remain after a failed checkpoint.
  int governancePolicyLockMonths() const { return m_governancePolicyLockMonths; }

private:
  FeatureFlags() = default;
  ~FeatureFlags() = default;

  bool m_affordabilitySystem  = false;
  bool m_heatmapOverlay       = false;
  bool m_jsonContentPipeline  = false;
  bool m_governanceLayer      = false;
  bool m_eventSystem          = false;
  bool m_councilCheckpoint    = false;
  bool m_cityIndicesDashboard = false;

  int m_governanceCheckpointMonths = 6;
  float m_governanceConstraintThreshold = 40.f;
  float m_governanceEventThreshold = 30.f;
  float m_governanceSoftFailThreshold = 15.f;
  int m_governancePolicyLockMonths = 3;
};

#endif // FEATURE_FLAGS_HXX_
