#include "PointCloud.hpp"

#include <algorithm>

namespace pointmod {

void Bounds::Expand(const Vec3& value) {
  min.x = std::min(min.x, value.x);
  min.y = std::min(min.y, value.y);
  min.z = std::min(min.z, value.z);
  max.x = std::max(max.x, value.x);
  max.y = std::max(max.y, value.y);
  max.z = std::max(max.z, value.z);
}

bool Bounds::IsValid() const {
  return min.x <= max.x && min.y <= max.y && min.z <= max.z;
}

Vec3 Bounds::Center() const {
  return {(min.x + max.x) * 0.5f, (min.y + max.y) * 0.5f, (min.z + max.z) * 0.5f};
}

Vec3 Bounds::Extents() const {
  return {max.x - min.x, max.y - min.y, max.z - min.z};
}

float Bounds::Radius() const {
  const Vec3 extents = Extents();
  return std::max({extents.x, extents.y, extents.z}) * 0.5f;
}

}  // namespace pointmod
