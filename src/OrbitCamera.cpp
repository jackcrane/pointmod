#include "OrbitCamera.hpp"

#include <algorithm>
#include <cmath>

namespace pointmod {

namespace {

constexpr float kHalfPi = 1.57079632679f;

Vec3 WorldUpVector(WorldUpAxis axis) {
  switch (axis) {
    case WorldUpAxis::kY:
      return {0.0f, 1.0f, 0.0f};
    case WorldUpAxis::kNegativeY:
      return {0.0f, -1.0f, 0.0f};
    case WorldUpAxis::kZ:
      return {0.0f, 0.0f, 1.0f};
    case WorldUpAxis::kNegativeZ:
      return {0.0f, 0.0f, -1.0f};
  }
  return {0.0f, 0.0f, 1.0f};
}

Vec3 WorldForwardReference(WorldUpAxis axis) {
  (void)axis;
  return {1.0f, 0.0f, 0.0f};
}

Vec3 WorldRightReference(WorldUpAxis axis) {
  return Normalize(Cross(WorldUpVector(axis), WorldForwardReference(axis)));
}

}  // namespace

void OrbitCamera::Frame(const Bounds& bounds) {
  if (!bounds.IsValid()) {
    return;
  }

  target_ = bounds.Center();
  orbitRadius_ = std::max(bounds.Radius(), 0.01f);
  distance_ = orbitRadius_ * 2.5f;
}

void OrbitCamera::ResetOrientation() {
  yawRadians_ = 0.35f;
  pitchRadians_ = 0.35f;
}

void OrbitCamera::SetWorldUpAxis(WorldUpAxis axis) {
  upAxis_ = axis;
}

void OrbitCamera::Rotate(float deltaX, float deltaY) {
  yawRadians_ -= deltaX * 0.0085f;
  pitchRadians_ += deltaY * 0.0085f;
  pitchRadians_ = std::clamp(pitchRadians_, -kHalfPi + 0.02f, kHalfPi - 0.02f);
}

void OrbitCamera::Pan(float deltaX, float deltaY) {
  const Vec3 eye = Position();
  const Vec3 forward = Normalize(target_ - eye);
  Vec3 right = Normalize(Cross(forward, WorldUp()));
  if (Length(right) <= 0.0f) {
    right = WorldRightReference(upAxis_);
  }
  const Vec3 up = Normalize(Cross(right, forward));

  const float panScale = std::max(distance_, 0.01f) * 0.0015f;
  target_ += right * (-deltaX * panScale);
  target_ += up * (deltaY * panScale);
}

void OrbitCamera::Zoom(float amount) {
  const float zoomScale = 1.0f - amount * 0.1f;
  distance_ = std::clamp(distance_ * zoomScale, orbitRadius_ * 0.01f, orbitRadius_ * 200.0f);
}

Mat4 OrbitCamera::ViewProjection(float aspectRatio) const {
  const Mat4 projection = Perspective(0.85f, aspectRatio, 0.01f, std::max(distance_ + orbitRadius_ * 6.0f, 50.0f));
  const Mat4 view = LookAt(Position(), target_, WorldUp());
  return Multiply(projection, view);
}

Vec3 OrbitCamera::Position() const {
  const float cosPitch = std::cos(pitchRadians_);
  const Vec3 worldForward = WorldForwardReference(upAxis_);
  const Vec3 worldRight = WorldRightReference(upAxis_);
  const Vec3 worldUp = WorldUp();
  return {
    target_.x + distance_ * (
      worldForward.x * cosPitch * std::cos(yawRadians_) +
      worldRight.x * cosPitch * std::sin(yawRadians_) +
      worldUp.x * std::sin(pitchRadians_)),
    target_.y + distance_ * (
      worldForward.y * cosPitch * std::cos(yawRadians_) +
      worldRight.y * cosPitch * std::sin(yawRadians_) +
      worldUp.y * std::sin(pitchRadians_)),
    target_.z + distance_ * (
      worldForward.z * cosPitch * std::cos(yawRadians_) +
      worldRight.z * cosPitch * std::sin(yawRadians_) +
      worldUp.z * std::sin(pitchRadians_)),
  };
}

Vec3 OrbitCamera::Target() const {
  return target_;
}

Vec3 OrbitCamera::WorldUp() const {
  return WorldUpVector(upAxis_);
}

WorldUpAxis OrbitCamera::UpAxis() const {
  return upAxis_;
}

}  // namespace pointmod
