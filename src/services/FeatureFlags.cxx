#include "FeatureFlags.hxx"

#include "../util/LOG.hxx"
#include "../util/Filesystem.hxx"
#include "../engine/common/Constants.hxx"

#include <json.hxx>

using json = nlohmann::json;

static constexpr const char FEATURE_FLAGS_FILE[] = "resources/data/FeatureFlags.json";

void FeatureFlags::readFile()
{
  const std::string path = fs::getBasePath() + FEATURE_FLAGS_FILE;

  if (!fs::fileExists(path))
  {
    LOG(LOG_WARNING) << "FeatureFlags.json not found at " << path << " – all features disabled.";
    return;
  }

  const std::string raw = fs::readFileAsString(path);
  const json j = json::parse(raw, nullptr, /*allow_exceptions=*/false);

  if (j.is_discarded())
  {
    LOG(LOG_ERROR) << "Failed to parse FeatureFlags.json – all features disabled.";
    return;
  }

  m_affordabilitySystem  = j.value("affordability_system",   false);
  m_heatmapOverlay       = j.value("heatmap_overlay",        false);
  m_jsonContentPipeline  = j.value("json_content_pipeline",  false);
  m_governanceLayer      = j.value("governance_layer",        false);
  m_eventSystem          = j.value("event_system",            false);
  m_councilCheckpoint    = j.value("council_checkpoint",      false);
  m_cityIndicesDashboard = j.value("city_indices_dashboard",  false);
  m_governanceCheckpointMonths = j.value("governance_checkpoint_months", 6);
  m_governanceConstraintThreshold = j.value("governance_constraint_threshold", 40.f);
  m_governanceEventThreshold = j.value("governance_event_threshold", 30.f);
  m_governanceSoftFailThreshold = j.value("governance_soft_fail_threshold", 15.f);
  m_governancePolicyLockMonths = j.value("governance_policy_lock_months", 3);

  LOG(LOG_INFO) << "FeatureFlags loaded:"
                << " affordability=" << m_affordabilitySystem
                << " heatmap="       << m_heatmapOverlay
                << " jsonPipeline="  << m_jsonContentPipeline
                << " governance="    << m_governanceLayer
                << " events="        << m_eventSystem
                << " council="       << m_councilCheckpoint
                << " dashboard="     << m_cityIndicesDashboard
                << " checkpointMonths=" << m_governanceCheckpointMonths
                << " constraintThreshold=" << m_governanceConstraintThreshold
                << " eventThreshold=" << m_governanceEventThreshold
                << " softFailThreshold=" << m_governanceSoftFailThreshold
                << " policyLockMonths=" << m_governancePolicyLockMonths;
}
