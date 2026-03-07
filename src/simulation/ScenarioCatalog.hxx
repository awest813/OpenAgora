#ifndef SCENARIO_CATALOG_HXX_
#define SCENARIO_CATALOG_HXX_

#include "Singleton.hxx"

#include <string>
#include <vector>

struct ScenarioDefinition
{
  std::string id;
  std::string label;
  std::string description;
  float startingApproval = 50.f;
  float startingBalance = 0.f;
  std::vector<std::string> recommendedPolicies;
};

class ScenarioCatalog : public Singleton<ScenarioCatalog>
{
public:
  friend Singleton<ScenarioCatalog>;

  void loadScenarios();
  void clear();

  const std::vector<ScenarioDefinition> &definitions() const { return m_definitions; }
  const ScenarioDefinition *find(const std::string &id) const;

private:
  ScenarioCatalog() = default;

  std::vector<ScenarioDefinition> m_definitions;
};

#endif // SCENARIO_CATALOG_HXX_
