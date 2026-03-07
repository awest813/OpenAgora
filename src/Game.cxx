#include "Game.hxx"
#include "engine/EventManager.hxx"
#include "engine/UIManager.hxx"
#include "engine/WindowManager.hxx"
#include "engine/basics/Camera.hxx"
#include "LOG.hxx"
#include "engine/basics/Settings.hxx"
#include "engine/basics/GameStates.hxx"
#include "Filesystem.hxx"
#include "OSystem.hxx"
#include <Map.hxx>
#include <MapFunctions.hxx>
#include "../game/ui/BuildMenu.hxx"
#include "../game/ui/GameTimeMenu.hxx"
#include "../game/ui/CityIndicesPanel.hxx"
#include "../game/ui/GovernancePanel.hxx"
#include "../game/ui/PolicyPanel.hxx"
#include "../game/ui/NotificationOverlay.hxx"
#include "../game/ui/EconomyPanel.hxx"
#include "../game/ui/EventLogPanel.hxx"
#include <CityIndices.hxx>
#include <GovernanceSystem.hxx>
#include <AffordabilityModel.hxx>
#include <PolicyEngine.hxx>
#include <BudgetSystem.hxx>
#include <EconomyDepthModel.hxx>
#include <ServiceStrainModel.hxx>
#include <ScenarioCatalog.hxx>
#include <SimulationContext.hxx>
#include "services/FeatureFlags.hxx"
#include "services/FrameMetrics.hxx"
#include "engine/GameObjects/MapNode.hxx"
#include "engine/common/enums.hxx"

#include <SDL.h>
#include <SDL_ttf.h>

#ifdef USE_ANGELSCRIPT
#include "scripting/ScriptEngine.hxx"
#endif

#ifdef USE_MOFILEREADER
#include "moFileReader.h"
#endif

#ifdef MICROPROFILE_ENABLED
#include "microprofile/microprofile.h"
#endif

namespace Cytopia
{
Game::Game()
{
  LOG(LOG_DEBUG) << "Created Game Object";
  initialize();
}

void Game::quit()
{
#ifdef USE_AUDIO
  auto &m_AudioMixer = AudioMixer::instance();
  m_AudioMixer.stopAll();
  if (!Settings::instance().audio3DStatus)
  {
    m_AudioMixer.play(SoundtrackID{"NegativeSelect"});
  }
  else
  {
    m_AudioMixer.play(SoundtrackID{"NegativeSelect"}, Coordinate3D{0, 0, -4});
  }
#endif // USE_AUDIO
}

void Game::initialize()
{
#ifdef USE_MOFILEREADER
  std::string moFilePath = fs::getBasePath();
  moFilePath = moFilePath + "languages/" + Settings::instance().gameLanguage + "/Cytopia.mo";

  if (moFileLib::moFileReaderSingleton::GetInstance().ReadFile(moFilePath.c_str()) == moFileLib::moFileReader::EC_SUCCESS)
  {
    LOG(LOG_INFO) << "Loaded MO file " << moFilePath;
  }
  else
  {
    LOG(LOG_ERROR) << "Failed to load MO file " << moFilePath;
  }
#endif

  // Register Callbacks
  SignalMediator::instance().registerCbQuitGame([this]() { m_shutDown = true; });
  SignalMediator::instance().registerCbNewGame(Signal::slot(this, &Game::newGame));
  SignalMediator::instance().registerCbLoadGame(Signal::slot(this, &Game::loadGame));

  // we need to instantiate the Map object to be able to send signals for new / load game
  MapFunctions::instance();

  LOG(LOG_DEBUG) << "Initialized Game Object";
}

void Game::run(bool SkipMenu)
{
  Camera::instance().centerScreenOnMapCenter();

  SDL_Event event;
  EventManager &evManager = EventManager::instance();

  UIManager &uiManager = UIManager::instance();
  uiManager.init();

  GameClock &gameClock = GameClock::instance();
  FeatureFlags::instance().readFile();
  const FeatureFlags &featureFlags = FeatureFlags::instance();

#ifdef USE_ANGELSCRIPT
  ScriptEngine &scriptEngine = ScriptEngine::instance();
  scriptEngine.init();
  scriptEngine.loadScript(fs::getBasePath() + "/resources/test.as", ScriptCategory::BUILD_IN);
#endif

#ifdef USE_AUDIO
  if (!Settings::instance().audio3DStatus)
  {
    gameClock.addRealTimeClockTask(
        []()
        {
          AudioMixer::instance().play(AudioTrigger::MainTheme);
          return false;
        },
        0s, 8min);
    gameClock.addRealTimeClockTask(
        []()
        {
          AudioMixer::instance().play(AudioTrigger::NatureSounds);
          return false;
        },
        0s, 3min);
  }
  else
  {
    gameClock.addRealTimeClockTask(
        []()
        {
          AudioMixer::instance().play(AudioTrigger::MainTheme, Coordinate3D{0, 0.5, 0.1});
          return false;
        },
        0s, 8min);
    gameClock.addRealTimeClockTask(
        []()
        {
          AudioMixer::instance().play(AudioTrigger::NatureSounds, Coordinate3D{0, 0, -2});
          return false;
        },
        0s, 3min);
  }
#endif // USE_AUDIO

  // FPS Counter variables
  const float fpsIntervall = 1.0; // interval the fps counter is refreshed in seconds.
  Uint32 fpsLastTime = SDL_GetTicks();
  Uint32 fpsFrames = 0;

  // High-resolution frame timing (used by the debug overlay).
  const Uint64 perfFreq = SDL_GetPerformanceFrequency();

  uiManager.addPersistentMenu<GameTimeMenu>();
  uiManager.addPersistentMenu<BuildMenu>();

  const bool governanceEnabled = featureFlags.governanceLayer();
  const bool affordabilityEnabled = featureFlags.affordabilitySystem();
  const bool jsonPipelineEnabled = featureFlags.jsonContentPipeline();
  const bool needsCityIndices = featureFlags.cityIndicesDashboard() || governanceEnabled || affordabilityEnabled;

  if (featureFlags.cityIndicesDashboard())
  {
    uiManager.addPersistentMenu<CityIndicesPanel>();
  }

  if (governanceEnabled)
  {
    GovernanceSystem::instance().configure(featureFlags.governanceCheckpointMonths(),
                                           featureFlags.governanceConstraintThreshold(),
                                           featureFlags.governanceEventThreshold(),
                                           featureFlags.governanceSoftFailThreshold(),
                                           featureFlags.governancePolicyLockMonths(), featureFlags.eventSystem(),
                                           featureFlags.councilCheckpoint());
    GovernanceSystem::instance().loadEventDefinitions();
    GovernanceSystem::instance().reset();
    uiManager.addPersistentMenu<GovernancePanel>();
  }

  if (affordabilityEnabled)
  {
    AffordabilityModel::instance().reset();
  }

  if (jsonPipelineEnabled)
  {
    PolicyEngine::instance().loadPolicies();
    ScenarioCatalog::instance().loadScenarios();
  }

  if (featureFlags.policyPanel())
  {
    uiManager.addPersistentMenu<PolicyPanel>();
  }

  if (featureFlags.notificationOverlay())
  {
    uiManager.addPersistentMenu<NotificationOverlay>();
  }

  if (featureFlags.economyPanel())
  {
    uiManager.addPersistentMenu<EconomyPanel>();
  }

  if (featureFlags.eventLogPanel())
  {
    uiManager.addPersistentMenu<EventLogPanel>();
  }

  if (featureFlags.budgetSystem())
  {
    BudgetSystem::instance().reset();
  }

  if (featureFlags.economyDepthModel())
  {
    EconomyDepthModel::instance().reset();
  }

  if (featureFlags.serviceStrainModel())
  {
    ServiceStrainModel::instance().reset();
  }

  SimulationContext::instance().reset();

  if (needsCityIndices)
  {
    // Tick the full simulation stack every in-game month (30 game days).
    gameClock.addGameTimeClockTask(
        [this]() -> bool
        {
          if (!MapFunctions::instance().getMap())
            return false;

          m_GamePlay.runMonthlySimulationTick(MapFunctions::instance().getMapNodes());
          return false;
        },
        30 * GameClock::GameDay,
        30 * GameClock::GameDay);
  }

  // GameLoop
  while (!m_shutDown)
  {
#ifdef MICROPROFILE_ENABLED
    MICROPROFILE_SCOPEI("Map", "Gameloop", MP_GREEN);
#endif
    const Uint64 frameStart = SDL_GetPerformanceCounter();

    SDL_RenderClear(WindowManager::instance().getRenderer());

    evManager.checkEvents(event);
    gameClock.tick();

    // render the tileMap
    if (MapFunctions::instance().getMap())
    {
      MapFunctions::instance().getMap()->renderMap();
    }

    // render the ui
    // TODO: This is only temporary until the new UI is ready. Remove this afterwards
    const Uint64 uiStart = SDL_GetPerformanceCounter();
    if (GameStates::instance().drawUI)
    {
      WindowManager::instance().newImGuiFrame();
      uiManager.drawUI();
    }
    const float uiMs = static_cast<float>(SDL_GetPerformanceCounter() - uiStart) * 1000.f / static_cast<float>(perfFreq);

#ifdef USE_ANGELSCRIPT
    ScriptEngine::instance().framestep(1);
#endif

    // we need to instantiate the MapFunctions object so it's ready for new game
    WindowManager::instance().renderScreen();

    // Record frame timing for the debug overlay.
    const float frameMs =
        static_cast<float>(SDL_GetPerformanceCounter() - frameStart) * 1000.f / static_cast<float>(perfFreq);
    FrameMetrics::instance().recordFrame(frameMs, uiMs);

    fpsFrames++;

    if (fpsLastTime < SDL_GetTicks() - fpsIntervall * 1000)
    {
      fpsLastTime = SDL_GetTicks();
      uiManager.setFPSCounterText("FPS: " + std::to_string(fpsFrames));
      fpsFrames = 0;
    }

#ifdef MICROPROFILE_ENABLED
    MicroProfileFlip(nullptr);
#endif
  }
}

void Game::shutdown()
{
  LOG(LOG_DEBUG) << "In shutdown";
  TTF_Quit();
  SDL_Quit();
}

void Game::newGame(bool generateTerrain)
{
  MapFunctions::instance().newMap(generateTerrain);
  m_GamePlay.resetManagers();
  GovernanceSystem::instance().reset();
  BudgetSystem::instance().reset();
  EconomyDepthModel::instance().reset();
  ServiceStrainModel::instance().reset();
  SimulationContext::instance().reset();
}

void Game::loadGame(const std::string &fileName)
{
  MapFunctions::instance().loadMapFromFile(fileName);
  m_GamePlay.resetManagers();
  GovernanceSystem::instance().reset();
  BudgetSystem::instance().reset();
  EconomyDepthModel::instance().reset();
  ServiceStrainModel::instance().reset();
  SimulationContext::instance().reset();
}

} // namespace Cytopia
