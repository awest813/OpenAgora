#include "ScenarioCatalog.hxx"

#include "Constants.hxx"
#include "Filesystem.hxx"
#include "LOG.hxx"
#include "json.hxx"

#include <algorithm>
#include <filesystem>

using json = nlohmann::json;

namespace
{
constexpr const char SCENARIOS_DIRECTORY[] = "resources/data/scenarios";
} // namespace

void ScenarioCatalog::loadScenarios()
{
  clear();

  const std::filesystem::path scenariosDir = std::filesystem::path(fs::getBasePath()) / SCENARIOS_DIRECTORY;
  if (!std::filesystem::exists(scenariosDir) || !std::filesystem::is_directory(scenariosDir))
  {
    LOG(LOG_WARNING) << "Scenario directory not found: " << scenariosDir.string();
    return;
  }

  std::vector<std::filesystem::path> files;
  for (const auto &entry : std::filesystem::directory_iterator(scenariosDir))
  {
    if (!entry.is_regular_file() || entry.path().extension() != ".json")
      continue;
    files.emplace_back(entry.path());
  }
  std::sort(files.begin(), files.end());

  for (const auto &path : files)
  {
    const std::string raw = fs::readFileAsString(path.string());
    const json parsed = json::parse(raw, nullptr, false);
    if (parsed.is_discarded())
    {
      LOG(LOG_WARNING) << "Skipping malformed scenario file: " << path.string();
      continue;
    }

    ScenarioDefinition definition;
    definition.id = parsed.value("id", path.stem().string());
    definition.label = parsed.value("label", definition.id);
    definition.description = parsed.value("description", std::string{});
    definition.startingApproval = parsed.value("starting_approval", 50.f);
    definition.startingBalance = parsed.value("starting_balance", 0.f);

    if (parsed.contains("recommended_policies") && parsed["recommended_policies"].is_array())
    {
      for (const auto &policy : parsed["recommended_policies"])
      {
        if (policy.is_string())
          definition.recommendedPolicies.push_back(policy.get<std::string>());
      }
    }

    m_definitions.push_back(std::move(definition));
  }

  LOG(LOG_INFO) << "ScenarioCatalog loaded " << m_definitions.size() << " scenario(s)";
}

void ScenarioCatalog::clear()
{
  m_definitions.clear();
}

const ScenarioDefinition *ScenarioCatalog::find(const std::string &id) const
{
  const auto it = std::find_if(m_definitions.begin(), m_definitions.end(),
                               [&id](const ScenarioDefinition &definition) { return definition.id == id; });
  if (it == m_definitions.end())
    return nullptr;
  return &(*it);
}
