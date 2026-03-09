#ifndef DIFFICULTY_SETTINGS_HXX_
#define DIFFICULTY_SETTINGS_HXX_

/// Difficulty presets available at new-game setup.
enum class DifficultyLevel : int
{
  Sandbox  = 0,
  Standard = 1,
  Challenge = 2
};

/// Parameters that GovernanceSystem uses, tuned per difficulty.
struct DifficultyConfig
{
  int   checkpointIntervalMonths = 6;
  float constraintThreshold      = 40.f; ///< approval below this → policy lock
  float eventThreshold           = 30.f; ///< approval below this → events fire
  float softFailThreshold        = 15.f; ///< approval below this → election lost
  int   policyLockMonths         = 3;
  bool  councilCheckpointEnabled = true;
  bool  sandboxMode              = false; ///< no election loss, no policy lock
};

/// Returns the tuned DifficultyConfig for the given level.
inline DifficultyConfig difficultyConfig(DifficultyLevel level) noexcept
{
  switch (level)
  {
  case DifficultyLevel::Sandbox:
    // Council checkpoints disabled → no approval gates, no election loss, no policy lock.
    return {/*checkpointIntervalMonths=*/6,
            /*constraintThreshold=*/40.f,
            /*eventThreshold=*/30.f,
            /*softFailThreshold=*/0.f,
            /*policyLockMonths=*/0,
            /*councilCheckpointEnabled=*/false,
            /*sandboxMode=*/true};
  case DifficultyLevel::Challenge:
    // Checkpoints every 4 months, tighter thresholds, longer policy lock.
    return {/*checkpointIntervalMonths=*/4,
            /*constraintThreshold=*/50.f,
            /*eventThreshold=*/40.f,
            /*softFailThreshold=*/25.f,
            /*policyLockMonths=*/4,
            /*councilCheckpointEnabled=*/true,
            /*sandboxMode=*/false};
  default: // Standard
    return {/*checkpointIntervalMonths=*/6,
            /*constraintThreshold=*/40.f,
            /*eventThreshold=*/30.f,
            /*softFailThreshold=*/15.f,
            /*policyLockMonths=*/3,
            /*councilCheckpointEnabled=*/true,
            /*sandboxMode=*/false};
  }
}

/// Human-readable label for a difficulty level.
inline const char *difficultyLabel(DifficultyLevel level) noexcept
{
  switch (level)
  {
  case DifficultyLevel::Sandbox:   return "Sandbox";
  case DifficultyLevel::Challenge: return "Challenge";
  default:                         return "Standard";
  }
}

/// Short description shown in the difficulty selector.
inline const char *difficultyDescription(DifficultyLevel level) noexcept
{
  switch (level)
  {
  case DifficultyLevel::Sandbox:
    return "No election loss. All policies always available. Unlimited budget.";
  case DifficultyLevel::Challenge:
    return "Tighter approval thresholds. Stricter budget. Events fire more frequently.";
  default:
    return "Normal council approval gates and budget constraints.";
  }
}

#endif // DIFFICULTY_SETTINGS_HXX_
