#ifndef SCENARIO_CATALOG_HXX_
#define SCENARIO_CATALOG_HXX_

#include "DifficultySettings.hxx"
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
  void addDefinition(const ScenarioDefinition &definition);

  const std::vector<ScenarioDefinition> &definitions() const { return m_definitions; }
  const ScenarioDefinition *find(const std::string &id) const;
  bool applyScenario(const ScenarioDefinition &definition) const;
  bool applyScenarioById(const std::string &id) const;

  /// Store the player's new-game selection (read by Game::newGame).
  void setPendingSelection(const std::string &scenarioId, DifficultyLevel difficulty);
  void clearPending();
  const std::string &pendingScenarioId() const { return m_pendingScenarioId; }
  DifficultyLevel pendingDifficulty() const { return m_pendingDifficulty; }

private:
  ScenarioCatalog() = default;

  std::vector<ScenarioDefinition> m_definitions;
  std::string m_pendingScenarioId;
  DifficultyLevel m_pendingDifficulty = DifficultyLevel::Standard;
};

#endif // SCENARIO_CATALOG_HXX_
