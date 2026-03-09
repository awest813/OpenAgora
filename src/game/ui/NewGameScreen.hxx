#ifndef NEW_GAME_SCREEN_HXX_
#define NEW_GAME_SCREEN_HXX_

#include "UIManager.hxx"
#include "../../simulation/DifficultySettings.hxx"

/**
 * @class NewGameScreen
 * @brief Modal screen for scenario and difficulty selection before starting a new game.
 *
 * Follows the same result-enum pattern as LoadMenu so MainMenu.cxx can drive it
 * with a simple switch statement.
 */
class NewGameScreen : public GameMenu
{
public:
  enum Result
  {
    e_none,
    e_close,
    e_start_game
  };

  NewGameScreen();
  void draw() const override;

  Result result() const { return m_result; }

private:
  mutable Result         m_result            = e_none;
  mutable int            m_selectedScenario  = 0;
  mutable DifficultyLevel m_selectedDifficulty = DifficultyLevel::Standard;
};

#endif // NEW_GAME_SCREEN_HXX_
