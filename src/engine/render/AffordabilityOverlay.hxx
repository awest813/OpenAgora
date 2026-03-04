#ifndef AFFORDABILITY_OVERLAY_HXX_
#define AFFORDABILITY_OVERLAY_HXX_

#include <vector>

class Sprite;
class MapNode;

/**
 * @namespace AffordabilityOverlay
 * @brief Per-tile affordability heatmap rendered on top of the isometric map.
 *
 * Colours each residential tile according to its zone density:
 *   LOW    → green  (affordable)
 *   MEDIUM → yellow (moderate)
 *   HIGH   → red    (unaffordable)
 *
 * Enabled by the heatmap_overlay feature flag (FeatureFlags.json).
 * Called from Map::renderMap() after the tile sprites are drawn so the tinted
 * overlay sits on top of the buildings layer without requiring an extra Layer
 * enum slot.
 */
namespace AffordabilityOverlay
{

/**
 * @brief Render the affordability heatmap over all visible residential tiles.
 *
 * @param visibleSprites Pointer to the first entry in Map::pMapNodesVisible.
 * @param visibleCount   Number of valid entries in that array.
 * @param mapNodes       Flat vector of all MapNodes (used to look up TileData).
 */
void render(Sprite *const *visibleSprites, int visibleCount, const std::vector<MapNode> &mapNodes);

} // namespace AffordabilityOverlay

#endif // AFFORDABILITY_OVERLAY_HXX_
