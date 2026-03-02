#include "PolicyEngine.hxx"

#include "Constants.hxx"
#include "Filesystem.hxx"
#include "LOG.hxx"
#include "json.hxx"

#include <algorithm>
#include <cctype>
#include <filesystem>

using json = nlohmann::json;

namespace
{
constexpr const char POLICIES_DIRECTORY[] = "resources/data/policies";
} // namespace

// ── Static helpers ────────────────────────────────────────────────────────────

std::string PolicyEngine::normalizedKey(const std::string &value)
{
  std::string out = value;
  std::transform(out.begin(), out.end(), out.begin(),
                 [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
  return out;
}

bool PolicyEngine::applyNumericOp(float &target, const std::string &op, float value)
{
  const std::string normalizedOp = normalizedKey(op);
  if (normalizedOp == "add")
  {
    target += value;
    return true;
  }
  if (normalizedOp == "multiply")
  {
    target *= value;
    return true;
  }
  if (normalizedOp == "set")
  {
    target = value;
    return true;
  }
  return false;
}

// ── Effect dispatch ───────────────────────────────────────────────────────────

bool PolicyEngine::applyEffectToAffordability(const PolicyEffect &effect, AffordabilityState &state)
{
  const std::string key = normalizedKey(effect.target);
  float *target = nullptr;

  if (key == "affordabilityindex" || key == "affordability")
    target = &state.affordabilityIndex;
  else if (key == "displacementpressure" || key == "displacement")
    target = &state.displacementPressure;
  else if (key == "medianrent" || key == "rent")
    target = &state.medianRent;
  else if (key == "medianincome" || key == "income")
    target = &state.medianIncome;
  else if (key == "landvalueproxy" || key == "landvalue")
    target = &state.landValueProxy;

  if (!target)
    return false;

  if (!applyNumericOp(*target, effect.op, effect.value))
    return false;

  // Clamp all affordability fields to [0, 100]
  *target = std::max(0.f, std::min(100.f, *target));
  return true;
}

bool PolicyEngine::applyEffectToCityIndices(const PolicyEffect &effect, CityIndicesData &indices)
{
  const std::string key = normalizedKey(effect.target);
  float *target = nullptr;

  if (key == "affordabilityindex" || key == "affordability")
    target = &indices.affordability;
  else if (key == "safetyindex" || key == "safety")
    target = &indices.safety;
  else if (key == "jobsindex" || key == "jobs" || key == "economyindex" || key == "economy")
    target = &indices.jobs;
  else if (key == "commuteindex" || key == "commute")
    target = &indices.commute;
  else if (key == "pollutionindex" || key == "pollution")
    target = &indices.pollution;

  if (!target)
    return false;

  if (!applyNumericOp(*target, effect.op, effect.value))
    return false;

  *target = std::max(0.f, std::min(100.f, *target));
  return true;
}

// ── Public API ────────────────────────────────────────────────────────────────

void PolicyEngine::loadPolicies()
{
  clearPolicies();

  const std::filesystem::path policiesDir =
      std::filesystem::path(fs::getBasePath()) / POLICIES_DIRECTORY;

  if (!std::filesystem::exists(policiesDir) || !std::filesystem::is_directory(policiesDir))
  {
    LOG(LOG_WARNING) << "Policies directory not found: " << policiesDir.string();
    return;
  }

  std::vector<std::filesystem::path> policyFiles;
  for (const auto &entry : std::filesystem::directory_iterator(policiesDir))
  {
    if (!entry.is_regular_file() || entry.path().extension() != ".json")
      continue;
    policyFiles.emplace_back(entry.path());
  }
  std::sort(policyFiles.begin(), policyFiles.end());

  for (const auto &filePath : policyFiles)
  {
    const std::string raw = fs::readFileAsString(filePath.string());
    const json parsed = json::parse(raw, nullptr, false);

    if (parsed.is_discarded())
    {
      LOG(LOG_WARNING) << "Skipping malformed policy file: " << filePath.string();
      continue;
    }

    PolicyDefinition def;
    def.id          = parsed.value("id",          filePath.stem().string());
    def.label       = parsed.value("label",        def.id);
    def.description = parsed.value("description",  std::string{});
    def.type        = parsed.value("type",         std::string{"budget_sink"});
    def.costPerMonth = parsed.value("cost_per_month", 0);

    if (parsed.contains("effects") && parsed["effects"].is_array())
    {
      for (const auto &effectJson : parsed["effects"])
      {
        PolicyEffect eff;
        eff.target = effectJson.value("target", std::string{});
        eff.op     = effectJson.value("op",     std::string{"add"});
        eff.value  = effectJson.value("value",  0.f);
        def.effects.emplace_back(eff);
      }
    }

    if (parsed.contains("ui_slider") && parsed["ui_slider"].is_object())
    {
      const auto &sl = parsed["ui_slider"];
      def.sliderMin  = sl.value("min",  0.f);
      def.sliderMax  = sl.value("max",  1.f);
      def.sliderStep = sl.value("step", 1.f);
    }

    m_definitions.emplace_back(def);
    m_active.push_back(false);
  }

  LOG(LOG_INFO) << "PolicyEngine loaded " << m_definitions.size() << " policy definition(s)";
}

bool PolicyEngine::setActive(const std::string &policyId, bool active)
{
  for (size_t i = 0; i < m_definitions.size(); ++i)
  {
    if (m_definitions[i].id == policyId)
    {
      m_active[i] = active;
      return true;
    }
  }
  return false;
}

bool PolicyEngine::isActive(const std::string &policyId) const
{
  for (size_t i = 0; i < m_definitions.size(); ++i)
  {
    if (m_definitions[i].id == policyId)
      return m_active[i];
  }
  return false;
}

int PolicyEngine::tick(AffordabilityState &affordabilityState, CityIndicesData &cityIndices)
{
  int totalCost = 0;

  for (size_t i = 0; i < m_definitions.size(); ++i)
  {
    if (!m_active[i])
      continue;

    const PolicyDefinition &def = m_definitions[i];
    totalCost += def.costPerMonth;

    for (const auto &effect : def.effects)
    {
      if (applyEffectToAffordability(effect, affordabilityState))
        continue;

      if (applyEffectToCityIndices(effect, cityIndices))
        continue;

      // zone_modifier targets (zoneDensityGrowthRate, landValueProxy) are
      // handled by GamePlay, not here; log at debug level to avoid noise.
      LOG(LOG_DEBUG) << "PolicyEngine: unhandled effect target '" << effect.target
                     << "' in policy '" << def.id << "'";
    }
  }

  return totalCost;
}

void PolicyEngine::clearPolicies()
{
  m_definitions.clear();
  m_active.clear();
}

void PolicyEngine::addDefinition(const PolicyDefinition &definition)
{
  m_definitions.push_back(definition);
  m_active.push_back(false);
}
