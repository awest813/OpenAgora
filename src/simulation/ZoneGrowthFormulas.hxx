#ifndef ZONE_GROWTH_FORMULAS_HXX_
#define ZONE_GROWTH_FORMULAS_HXX_

#include "CityIndices.hxx"
#include "SimulationContext.hxx"

/**
 * @brief Per-zone-type growth multipliers computed from economy + city indices.
 *
 * Values of 1.0 are neutral.  Values > 1.0 accelerate that zone type and
 * values < 1.0 slow it.  All values are clamped to [0, 2].
 */
struct ZoneGrowthMultipliers
{
  float residential = 1.f; ///< Driven by affordability index
  float commercial  = 1.f; ///< Driven by business confidence + unemployment
  float industrial  = 1.f; ///< Driven by pollution index + debt stress
};

/**
 * @brief Compute per-zone-type growth multipliers from current simulation state.
 *
 * This is a pure function – it reads the provided snapshots and produces
 * multipliers that can be fed directly to ZoneManager::setZoneTypeGrowthMultipliers().
 *
 * @param indices         Latest city indices snapshot.
 * @param ctx             Latest simulation context snapshot.
 * @param useEconomyDepth When false, falls back to index-only logic for
 *                        commercial growth (economy depth model is disabled).
 * @return Clamped per-zone-type multipliers.
 */
inline ZoneGrowthMultipliers computeZoneGrowthMultipliers(const CityIndicesData &indices,
                                                          const SimulationContextData &ctx,
                                                          bool useEconomyDepth)
{
  ZoneGrowthMultipliers out;

  // ── Residential ──────────────────────────────────────────────────────────
  // Scales with affordability: high affordability → people want to move in.
  {
    const float aff = indices.affordability;
    if (aff >= 65.f)
      out.residential = 1.15f;
    else if (aff >= 50.f)
      out.residential = 1.05f;
    else if (aff < 30.f)
      out.residential = 0.80f;
    else if (aff < 40.f)
      out.residential = 0.90f;
    // else 40-50: neutral 1.0
  }

  // ── Commercial ───────────────────────────────────────────────────────────
  // Scales with business confidence when economy depth is available, otherwise
  // falls back to jobs index.
  if (useEconomyDepth)
  {
    const float confidence   = ctx.businessConfidence;
    const float unemployment = ctx.unemploymentPressure;

    if (confidence >= 65.f)
      out.commercial = 1.15f;
    else if (confidence >= 50.f)
      out.commercial = 1.05f;
    else if (confidence < 35.f)
      out.commercial = 0.85f;
    else if (confidence < 50.f)
      out.commercial = 0.92f;
    // else 50-65: neutral 1.0

    // Extra penalty when unemployment is very high
    if (unemployment > 70.f)
      out.commercial *= 0.90f;
  }
  else
  {
    const float jobs = indices.jobs;
    if (jobs >= 65.f)
      out.commercial = 1.10f;
    else if (jobs < 35.f)
      out.commercial = 0.90f;
  }

  // ── Industrial ───────────────────────────────────────────────────────────
  // High pollution and high debt stress discourage new industrial investment.
  {
    const float pollution = indices.pollution;
    if (pollution > 70.f)
      out.industrial = 0.80f;
    else if (pollution > 55.f)
      out.industrial = 0.90f;
    else if (pollution < 30.f)
      out.industrial = 1.10f;
    // else 30-55: neutral 1.0

    if (useEconomyDepth && ctx.debtStress > 60.f)
      out.industrial *= 0.85f;
  }

  // Clamp all outputs to a sensible range
  auto clamp2 = [](float v) { return v < 0.f ? 0.f : (v > 2.f ? 2.f : v); };
  out.residential = clamp2(out.residential);
  out.commercial  = clamp2(out.commercial);
  out.industrial  = clamp2(out.industrial);

  return out;
}

#endif // ZONE_GROWTH_FORMULAS_HXX_
