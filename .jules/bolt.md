## 2024-03-15 - [Avoid std::vector allocation in inner loops for linear area checking]
**Learning:** In the `getMaximumTileSize` function (and potentially others), generating a `std::vector<Point>` via `PointFunctions::getArea` for straight 1D lines causes significant heap allocation overhead when repeatedly evaluated for every distance step and zone node.
**Action:** When evaluating membership along straight horizontal or vertical lines (common in bounding box/tile checks), use inline integer `for` loops instead of allocating vector containers.
