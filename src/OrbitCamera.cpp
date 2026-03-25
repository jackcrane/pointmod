#include "OrbitCamera.hpp"

#include <algorithm>
#include <cmath>

namespace pointmod {

namespace {

constexpr float kHalfPi = 1.57079632679f;
constexpr Vec3 kWorldUp = {0.0f, 0.0f, 1.0f};

}  // namespace

void OrbitCamera::Frame(const Bounds& bounds) {
  if (!bounds.IsValid()) {
    return;
  }

  target_ = bounds.Center();
  orbitRadius_ = std::max(bounds.Radius(), 0.01f);
  distance_ = orbitRadius_ * 2.5f;
}

void OrbitCamera::Rotate(float deltaX, float deltaY) {
  yawRadians_ -= deltaX * 0.0085f;
  pitchRadians_ -= deltaY * 0.0085f;
  pitchRadians_ = std::clamp(pitchRadians_, -kHalfPi + 0.02f, kHalfPi - 0.02f);
}

void OrbitCamera::Pan(float deltaX, float deltaY) {
  const Vec3 eye = Position();
  const Vec3 forward = Normalize(target_ - eye);
  const Vec3 right = Normalize(Cross(forward, kWorldUp));
  const Vec3 up = Normalize(Cross(right, forward));

  const float panScale = std::max(distance_, 0.01f) * 0.0015f;
  target_ += right * (-deltaX * panScale);
  target_ += up * (deltaY * panScale);
}

void OrbitCamera::Zoom(float amount) {
  const float zoomScale = 1.0f - amount * 0.1f;
  distance_ = std::clamp(distance_ * zoomScale, orbitRadius_ * 0.05f, orbitRadius_ * 200.0f);
}

Mat4 OrbitCamera::ViewProjection(float aspectRatio) const {
  const Mat4 projection = Perspective(0.85f, aspectRatio, 0.01f, std::max(distance_ + orbitRadius_ * 6.0f, 50.0f));
  const Mat4 view = LookAt(Position(), target_, kWorldUp);
  return Multiply(projection, view);
}

Vec3 OrbitCamera::Position() const {
  const float cosPitch = std::cos(pitchRadians_);
  return {
    target_.x + distance_ * cosPitch * std::cos(yawRadians_),
    target_.y + distance_ * cosPitch * std::sin(yawRadians_),
    target_.z + distance_ * std::sin(pitchRadians_),
  };
}

Vec3 OrbitCamera::Target() const {
  return target_;
}

}  // namespace pointmod
