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

bool PolicyEngine::isSupportedTarget(const std::string &target)
{
  const std::string key = normalizedKey(target);
  return key == "affordabilityindex" || key == "affordability" ||
         key == "displacementpressure" || key == "displacement" ||
         key == "medianrent" || key == "rent" ||
         key == "medianincome" || key == "income" ||
         key == "landvalueproxy" || key == "landvalue" ||
         key == "safetyindex" || key == "safety" ||
         key == "jobsindex" || key == "jobs" || key == "economyindex" || key == "economy" ||
         key == "commuteindex" || key == "commute" ||
         key == "pollutionindex" || key == "pollution" ||
         key == "taxefficiency" ||
         key == "approvalmultiplier" ||
         key == "growthratemodifier" || key == "zonedensitygrowthrate" || key == "zonedensitygrowth" ||
         key == "unemploymentpressure" ||
         key == "wagepressure" ||
         key == "businessconfidence" ||
         key == "debtstress" ||
         key == "transitreliability" ||
         key == "safetycapacityload" ||
         key == "educationaccessstress" ||
         key == "healthaccessstress";
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

bool PolicyEngine::applyEffectToSimulationContext(const PolicyEffect &effect, SimulationContextData &context)
{
  const std::string key = normalizedKey(effect.target);
  float *target = nullptr;

  if (key == "taxefficiency")
    target = &context.taxEfficiency;
  else if (key == "approvalmultiplier")
    target = &context.approvalMultiplier;
  else if (key == "growthratemodifier" || key == "zonedensitygrowthrate" || key == "zonedensitygrowth")
    target = &context.growthRateModifier;
  else if (key == "unemploymentpressure")
    target = &context.unemploymentPressure;
  else if (key == "wagepressure")
    target = &context.wagePressure;
  else if (key == "businessconfidence")
    target = &context.businessConfidence;
  else if (key == "debtstress")
    target = &context.debtStress;
  else if (key == "transitreliability")
    target = &context.transitReliability;
  else if (key == "safetycapacityload")
    target = &context.safetyCapacityLoad;
  else if (key == "educationaccessstress")
    target = &context.educationAccessStress;
  else if (key == "healthaccessstress")
    target = &context.healthAccessStress;

  if (!target)
    return false;

  if (!applyNumericOp(*target, effect.op, effect.value))
    return false;

  if (key == "taxefficiency" || key == "approvalmultiplier" || key == "growthratemodifier" ||
      key == "zonedensitygrowthrate" || key == "zonedensitygrowth")
  {
    *target = std::max(0.f, std::min(5.f, *target));
  }
  else
  {
    *target = std::max(0.f, std::min(100.f, *target));
  }
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
    def.category    = parsed.value("category",     std::string{"general"});
    def.type        = parsed.value("type",         std::string{"budget_sink"});
    def.costPerMonth = parsed.value("cost_per_month", 0);
    def.minApproval = parsed.value("min_approval", 0.f);
    def.durationMonths = std::max(0, parsed.value("duration_months", 0));

    if (parsed.contains("requires") && parsed["requires"].is_array())
    {
      for (const auto &value : parsed["requires"])
      {
        if (value.is_string())
          def.requires.push_back(value.get<std::string>());
      }
    }
    if (parsed.contains("exclusive_with") && parsed["exclusive_with"].is_array())
    {
      for (const auto &value : parsed["exclusive_with"])
      {
        if (value.is_string())
          def.exclusiveWith.push_back(value.get<std::string>());
      }
    }

    if (parsed.contains("effects") && parsed["effects"].is_array())
    {
      for (const auto &effectJson : parsed["effects"])
      {
        PolicyEffect eff;
        eff.target = effectJson.value("target", std::string{});
        eff.op     = effectJson.value("op",     std::string{"add"});
        eff.value  = effectJson.value("value",  0.f);
        if (isSupportedTarget(eff.target))
          def.effects.emplace_back(eff);
        else
          LOG(LOG_WARNING) << "PolicyEngine: unsupported target '" << eff.target << "' in " << filePath.string();
      }
    }

    if (parsed.contains("levels") && parsed["levels"].is_array())
    {
      for (size_t levelIndex = 0; levelIndex < parsed["levels"].size(); ++levelIndex)
      {
        const auto &levelJson = parsed["levels"][levelIndex];
        if (!levelJson.is_object())
          continue;

        PolicyLevelDefinition level;
        level.level = std::max(1, levelJson.value("level", static_cast<int>(levelIndex + 1)));
        level.costPerMonth = levelJson.value("cost_per_month", def.costPerMonth);
        level.unlockMinApproval = levelJson.value("unlock_min_approval", def.minApproval);

        if (levelJson.contains("effects") && levelJson["effects"].is_array())
        {
          for (const auto &effectJson : levelJson["effects"])
          {
            PolicyEffect effect;
            effect.target = effectJson.value("target", std::string{});
            effect.op     = effectJson.value("op", std::string{"add"});
            effect.value  = effectJson.value("value", 0.f);
            if (isSupportedTarget(effect.target))
              level.effects.push_back(effect);
            else
              LOG(LOG_WARNING) << "PolicyEngine: unsupported level target '" << effect.target << "' in " << filePath.string();
          }
        }

        def.levels.emplace_back(std::move(level));
      }
    }

    if (parsed.contains("ui_slider") && parsed["ui_slider"].is_object())
    {
      const auto &sl = parsed["ui_slider"];
      def.sliderMin  = sl.value("min",  0.f);
      def.sliderMax  = sl.value("max",  1.f);
      def.sliderStep = sl.value("step", 1.f);
    }

    if (def.levels.empty())
    {
      PolicyLevelDefinition defaultLevel;
      defaultLevel.level = 1;
      defaultLevel.costPerMonth = def.costPerMonth;
      defaultLevel.unlockMinApproval = def.minApproval;
      defaultLevel.effects = def.effects;
      def.levels.push_back(defaultLevel);
    }
    else
    {
      std::sort(def.levels.begin(), def.levels.end(),
                [](const PolicyLevelDefinition &lhs, const PolicyLevelDefinition &rhs) { return lhs.level < rhs.level; });
    }

    if (!parsed.contains("ui_slider"))
    {
      def.sliderMin = 0.f;
      def.sliderMax = static_cast<float>(def.levels.size());
      def.sliderStep = 1.f;
    }

    m_definitions.emplace_back(def);
    m_selectedLevel.push_back(0);
    m_remainingDurationMonths.push_back(0);
  }

  LOG(LOG_INFO) << "PolicyEngine loaded " << m_definitions.size() << " policy definition(s)";
}

bool PolicyEngine::setActive(const std::string &policyId, bool active)
{
  return setPolicyLevel(policyId, active ? 1 : 0);
}

bool PolicyEngine::isActive(const std::string &policyId) const
{
  return policyLevel(policyId) > 0;
}

bool PolicyEngine::setPolicyLevel(const std::string &policyId, int level)
{
  const int idx = findPolicyIndex(policyId);
  if (idx < 0 || level < 0)
    return false;

  const int maxLvl = maxLevel(policyId);
  if (level > maxLvl)
    return false;

  m_selectedLevel[static_cast<size_t>(idx)] = level;
  if (level <= 0)
    m_remainingDurationMonths[static_cast<size_t>(idx)] = 0;
  else if (m_definitions[static_cast<size_t>(idx)].durationMonths > 0)
    m_remainingDurationMonths[static_cast<size_t>(idx)] = m_definitions[static_cast<size_t>(idx)].durationMonths;

  return true;
}

int PolicyEngine::policyLevel(const std::string &policyId) const
{
  const int idx = findPolicyIndex(policyId);
  if (idx < 0)
    return 0;
  return m_selectedLevel[static_cast<size_t>(idx)];
}

int PolicyEngine::maxLevel(const std::string &policyId) const
{
  const int idx = findPolicyIndex(policyId);
  if (idx < 0)
    return 0;

  const auto &levels = m_definitions[static_cast<size_t>(idx)].levels;
  if (levels.empty())
    return 0;
  return std::max_element(levels.begin(), levels.end(),
                          [](const PolicyLevelDefinition &lhs, const PolicyLevelDefinition &rhs)
                          { return lhs.level < rhs.level; })
      ->level;
}

PolicyAvailability PolicyEngine::availability(const std::string &policyId, int requestedLevel, float approval) const
{
  const int idx = findPolicyIndex(policyId);
  if (idx < 0)
    return {false, "Unknown policy"};

  if (requestedLevel <= 0)
    return {};

  const PolicyDefinition &def = m_definitions[static_cast<size_t>(idx)];
  const PolicyLevelDefinition *levelDef = resolveLevelDefinition(def, requestedLevel);
  if (!levelDef)
    return {false, "Invalid level"};

  const float approvalGate = std::max(def.minApproval, levelDef->unlockMinApproval);
  if (approval < approvalGate)
    return {false, "Approval too low"};

  for (const auto &required : def.requires)
  {
    if (!isActive(required))
      return {false, "Missing prerequisite: " + required};
  }

  for (const auto &exclusiveId : def.exclusiveWith)
  {
    if (isActive(exclusiveId))
      return {false, "Conflicts with: " + exclusiveId};
  }

  return {};
}

int PolicyEngine::tick(AffordabilityState &affordabilityState, CityIndicesData &cityIndices, SimulationContextData *simContext)
{
  int totalCost = 0;

  for (size_t i = 0; i < m_definitions.size(); ++i)
  {
    const int level = m_selectedLevel[i];
    if (level <= 0)
      continue;

    const PolicyDefinition &def = m_definitions[i];
    const PolicyLevelDefinition *levelDef = resolveLevelDefinition(def, level);
    if (!levelDef)
      continue;

    totalCost += levelDef->costPerMonth;

    for (const auto &effect : levelDef->effects)
    {
      if (applyEffectToAffordability(effect, affordabilityState))
        continue;

      if (applyEffectToCityIndices(effect, cityIndices))
        continue;

      if (simContext && applyEffectToSimulationContext(effect, *simContext))
        continue;

      LOG(LOG_DEBUG) << "PolicyEngine: unhandled effect target '" << effect.target
                     << "' in policy '" << def.id << "'";
    }

    if (def.durationMonths > 0 && m_remainingDurationMonths[i] > 0)
    {
      --m_remainingDurationMonths[i];
      if (m_remainingDurationMonths[i] <= 0)
      {
        m_selectedLevel[i] = 0;
      }
    }
  }

  return totalCost;
}

void PolicyEngine::clearPolicies()
{
  m_definitions.clear();
  m_selectedLevel.clear();
  m_remainingDurationMonths.clear();
}

void PolicyEngine::addDefinition(const PolicyDefinition &definition)
{
  PolicyDefinition copy = definition;
  if (copy.levels.empty())
  {
    PolicyLevelDefinition defaultLevel;
    defaultLevel.level = 1;
    defaultLevel.costPerMonth = copy.costPerMonth;
    defaultLevel.unlockMinApproval = copy.minApproval;
    defaultLevel.effects = copy.effects;
    copy.levels.push_back(defaultLevel);
  }
  m_definitions.push_back(std::move(copy));
  m_selectedLevel.push_back(0);
  m_remainingDurationMonths.push_back(0);
}

const PolicyLevelDefinition *PolicyEngine::resolveLevelDefinition(const PolicyDefinition &definition, int level) const
{
  const auto it = std::find_if(definition.levels.begin(), definition.levels.end(),
                               [level](const PolicyLevelDefinition &entry) { return entry.level == level; });
  if (it == definition.levels.end())
    return nullptr;
  return &(*it);
}

int PolicyEngine::findPolicyIndex(const std::string &policyId) const
{
  for (size_t i = 0; i < m_definitions.size(); ++i)
  {
    if (m_definitions[i].id == policyId)
      return static_cast<int>(i);
  }
  return -1;
}
