#pragma once

#include "Math.hpp"

namespace pointmod {

class OrbitCamera {
 public:
  void Frame(const Bounds& bounds);
  void Rotate(float deltaX, float deltaY);
  void Pan(float deltaX, float deltaY);
  void Zoom(float amount);

  [[nodiscard]] Mat4 ViewProjection(float aspectRatio) const;
  [[nodiscard]] Vec3 Position() const;
  [[nodiscard]] Vec3 Target() const;

 private:
  Vec3 target_ = {0.0f, 0.0f, 0.0f};
  float yawRadians_ = 0.35f;
  float pitchRadians_ = 0.35f;
  float distance_ = 6.0f;
  float orbitRadius_ = 1.0f;
};

}  // namespace pointmod
