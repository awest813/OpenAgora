#include <catch.hpp>

#include "../../src/simulation/DifficultySettings.hxx"
#include "../../src/simulation/GovernanceSystem.hxx"
#include "../../src/simulation/ScenarioCatalog.hxx"

TEST_CASE("DifficultySettings: Sandbox config disables council checkpoint and soft fail",
          "[simulation][difficulty]")
{
  const DifficultyConfig cfg = difficultyConfig(DifficultyLevel::Sandbox);
  CHECK(cfg.councilCheckpointEnabled == false);
  CHECK(cfg.softFailThreshold == Approx(0.f));
  CHECK(cfg.policyLockMonths == 0);
  CHECK(cfg.sandboxMode == true);
}

TEST_CASE("DifficultySettings: Standard config uses default governance thresholds",
          "[simulation][difficulty]")
{
  const DifficultyConfig cfg = difficultyConfig(DifficultyLevel::Standard);
  CHECK(cfg.councilCheckpointEnabled == true);
  CHECK(cfg.sandboxMode == false);
  CHECK(cfg.checkpointIntervalMonths == 6);
  CHECK(cfg.constraintThreshold == Approx(40.f));
  CHECK(cfg.softFailThreshold == Approx(15.f));
  CHECK(cfg.policyLockMonths == 3);
}

TEST_CASE("DifficultySettings: Challenge config has tighter thresholds and shorter interval",
          "[simulation][difficulty]")
{
  const DifficultyConfig cfg = difficultyConfig(DifficultyLevel::Challenge);
  CHECK(cfg.councilCheckpointEnabled == true);
  CHECK(cfg.sandboxMode == false);
  CHECK(cfg.checkpointIntervalMonths == 4);
  CHECK(cfg.constraintThreshold > 40.f);
  CHECK(cfg.softFailThreshold > 15.f);
  CHECK(cfg.policyLockMonths >= 3);
}

TEST_CASE("DifficultySettings: Challenge is strictly harder than Standard",
          "[simulation][difficulty]")
{
  const DifficultyConfig standard  = difficultyConfig(DifficultyLevel::Standard);
  const DifficultyConfig challenge = difficultyConfig(DifficultyLevel::Challenge);
  // Checkpoints fire more often in Challenge.
  CHECK(challenge.checkpointIntervalMonths < standard.checkpointIntervalMonths);
  // It is harder to avoid policy lock and election loss in Challenge.
  CHECK(challenge.constraintThreshold > standard.constraintThreshold);
  CHECK(challenge.softFailThreshold   > standard.softFailThreshold);
}

TEST_CASE("DifficultySettings: labels are non-empty and distinct", "[simulation][difficulty]")
{
  const char *sandboxLabel   = difficultyLabel(DifficultyLevel::Sandbox);
  const char *standardLabel  = difficultyLabel(DifficultyLevel::Standard);
  const char *challengeLabel = difficultyLabel(DifficultyLevel::Challenge);

  CHECK(sandboxLabel   != nullptr);
  CHECK(standardLabel  != nullptr);
  CHECK(challengeLabel != nullptr);
  CHECK(std::string(sandboxLabel)   != std::string(standardLabel));
  CHECK(std::string(standardLabel)  != std::string(challengeLabel));
  CHECK(std::string(sandboxLabel)   != std::string(challengeLabel));
}

TEST_CASE("DifficultySettings: descriptions are non-empty", "[simulation][difficulty]")
{
  CHECK(std::string(difficultyDescription(DifficultyLevel::Sandbox)).size()   > 0);
  CHECK(std::string(difficultyDescription(DifficultyLevel::Standard)).size()  > 0);
  CHECK(std::string(difficultyDescription(DifficultyLevel::Challenge)).size() > 0);
}

TEST_CASE("ScenarioCatalog: pending selection stores and clears correctly",
          "[simulation][difficulty][scenario]")
{
  ScenarioCatalog &catalog = ScenarioCatalog::instance();
  catalog.clearPending();

  CHECK(catalog.pendingScenarioId().empty());
  CHECK(catalog.pendingDifficulty() == DifficultyLevel::Standard);

  catalog.setPendingSelection("fiscal_emergency", DifficultyLevel::Challenge);
  CHECK(catalog.pendingScenarioId() == "fiscal_emergency");
  CHECK(catalog.pendingDifficulty() == DifficultyLevel::Challenge);

  catalog.clearPending();
  CHECK(catalog.pendingScenarioId().empty());
  CHECK(catalog.pendingDifficulty() == DifficultyLevel::Standard);
}

TEST_CASE("DifficultySettings: GovernanceSystem respects Sandbox config (no checkpoint)",
          "[simulation][difficulty]")
{
  const DifficultyConfig cfg = difficultyConfig(DifficultyLevel::Sandbox);
  GovernanceSystem &gov = GovernanceSystem::instance();
  gov.configure(cfg.checkpointIntervalMonths, cfg.constraintThreshold, cfg.eventThreshold,
                cfg.softFailThreshold, cfg.policyLockMonths,
                /*eventSystem=*/false, cfg.councilCheckpointEnabled);
  gov.reset();

  // In sandbox mode the approval can drop very low without triggering election loss
  // because council checkpoints are disabled.
  CityIndicesData indices;
  indices.affordability = 10.f;
  indices.safety        = 10.f;
  indices.jobs          = 10.f;
  indices.commute       = 10.f;
  indices.pollution     = 90.f;

  // Tick for well more than a normal checkpoint interval.
  for (int i = 0; i < 12; ++i)
    gov.tickMonth(indices);

  // Election should NOT be lost in sandbox mode regardless of how low approval falls.
  CHECK(gov.lostElection() == false);
}
