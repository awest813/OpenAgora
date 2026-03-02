#ifndef CITY_INDICES_PANEL_HXX_
#define CITY_INDICES_PANEL_HXX_

#include "UIManager.hxx"

/**
 * @class CityIndicesPanel
 * @brief Persistent ImGui sidebar showing the five city-wide scalar indices.
 *
 * Reads directly from CityIndices::instance() each frame; no signal plumbing
 * required because ImGui renders every frame anyway.
 */
struct CityIndicesPanel : public GameMenu
{
  void draw() const override;
};

#endif // CITY_INDICES_PANEL_HXX_
