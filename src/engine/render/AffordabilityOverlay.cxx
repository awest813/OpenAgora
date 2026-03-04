#include "AffordabilityOverlay.hxx"

#include <Sprite.hxx>
#include <MapNode.hxx>
#include <WindowManager.hxx>
#include <enums.hxx>
#include <tileData.hxx>

#include <SDL.h>

namespace
{

struct HeatColor
{
  uint8_t r, g, b, a;
};

// Semi-transparent colour for each residential density tier.
constexpr HeatColor LOW_HEAT    = {50,  200,  50, 80}; ///< Green  – affordable
constexpr HeatColor MEDIUM_HEAT = {220, 180,  50, 80}; ///< Yellow – moderate
constexpr HeatColor HIGH_HEAT   = {220,  60,  60, 80}; ///< Red    – unaffordable

/**
 * @brief Pick the most expensive density colour present in a tile's zoneDensity list.
 *
 * If any HIGH entry exists the tile is coloured red; if any MEDIUM exists but no
 * HIGH it is coloured yellow; otherwise green.
 */
HeatColor densityColor(const TileData *td)
{
  HeatColor col = LOW_HEAT;
  for (const auto &d : td->zoneDensity)
  {
    if (d == +ZoneDensity::HIGH)
      return HIGH_HEAT;
    if (d == +ZoneDensity::MEDIUM)
      col = MEDIUM_HEAT;
  }
  return col;
}

} // namespace

void AffordabilityOverlay::render(Sprite *const *visibleSprites, int visibleCount, const std::vector<MapNode> &mapNodes)
{
  SDL_Renderer *renderer = WindowManager::instance().getRenderer();

  // Save current blend mode and restore it after drawing the overlay so that
  // subsequent SDL rendering calls are not affected by our BLEND state.
  SDL_BlendMode previousBlendMode = SDL_BLENDMODE_NONE;
  SDL_GetRenderDrawBlendMode(renderer, &previousBlendMode);
  SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

  for (int i = 0; i < visibleCount; ++i)
  {
    Sprite *sprite = visibleSprites[i];

    const int idx = sprite->isoCoordinates.toIndex();
    if (idx < 0 || idx >= static_cast<int>(mapNodes.size()))
      continue;

    const TileData *td = mapNodes[static_cast<size_t>(idx)].getTileData(Layer::BUILDINGS);
    if (!td || td->category != "Residential")
      continue;

    const HeatColor col = densityColor(td);
    const SDL_Rect rect = sprite->getDestRect(Layer::TERRAIN);

    SDL_SetRenderDrawColor(renderer, col.r, col.g, col.b, col.a);
    SDL_RenderFillRect(renderer, &rect);
  }

  SDL_SetRenderDrawBlendMode(renderer, previousBlendMode);
}
