#pragma once

#include "PointCloud.hpp"

#include <cmath>

namespace pointmod {

struct Mat4 {
  float m[16] = {
    1.0f, 0.0f, 0.0f, 0.0f,
    0.0f, 1.0f, 0.0f, 0.0f,
    0.0f, 0.0f, 1.0f, 0.0f,
    0.0f, 0.0f, 0.0f, 1.0f,
  };
};

inline Vec3 operator+(const Vec3& a, const Vec3& b) {
  return {a.x + b.x, a.y + b.y, a.z + b.z};
}

inline Vec3 operator-(const Vec3& a, const Vec3& b) {
  return {a.x - b.x, a.y - b.y, a.z - b.z};
}

inline Vec3 operator*(const Vec3& a, float scalar) {
  return {a.x * scalar, a.y * scalar, a.z * scalar};
}

inline Vec3 operator/(const Vec3& a, float scalar) {
  return {a.x / scalar, a.y / scalar, a.z / scalar};
}

inline Vec3& operator+=(Vec3& a, const Vec3& b) {
  a.x += b.x;
  a.y += b.y;
  a.z += b.z;
  return a;
}

inline float Dot(const Vec3& a, const Vec3& b) {
  return a.x * b.x + a.y * b.y + a.z * b.z;
}

inline Vec3 Cross(const Vec3& a, const Vec3& b) {
  return {
    a.y * b.z - a.z * b.y,
    a.z * b.x - a.x * b.z,
    a.x * b.y - a.y * b.x,
  };
}

inline float Length(const Vec3& value) {
  return std::sqrt(Dot(value, value));
}

inline Vec3 Normalize(const Vec3& value) {
  const float length = Length(value);
  if (length <= 0.0f) {
    return {0.0f, 0.0f, 0.0f};
  }
  return value / length;
}

inline Mat4 Identity() {
  return {};
}

inline Mat4 Multiply(const Mat4& a, const Mat4& b) {
  Mat4 result{};
  for (int column = 0; column < 4; ++column) {
    for (int row = 0; row < 4; ++row) {
      result.m[column * 4 + row] =
        a.m[0 * 4 + row] * b.m[column * 4 + 0] +
        a.m[1 * 4 + row] * b.m[column * 4 + 1] +
        a.m[2 * 4 + row] * b.m[column * 4 + 2] +
        a.m[3 * 4 + row] * b.m[column * 4 + 3];
    }
  }
  return result;
}

inline Mat4 Perspective(float fovRadians, float aspectRatio, float nearPlane, float farPlane) {
  const float tanHalfFov = std::tan(fovRadians * 0.5f);
  Mat4 result{};
  result.m[0] = 1.0f / (aspectRatio * tanHalfFov);
  result.m[5] = 1.0f / tanHalfFov;
  result.m[10] = -(farPlane + nearPlane) / (farPlane - nearPlane);
  result.m[11] = -1.0f;
  result.m[14] = -(2.0f * farPlane * nearPlane) / (farPlane - nearPlane);
  result.m[15] = 0.0f;
  return result;
}

inline Mat4 LookAt(const Vec3& eye, const Vec3& target, const Vec3& worldUp) {
  const Vec3 forward = Normalize(target - eye);
  const Vec3 right = Normalize(Cross(forward, worldUp));
  const Vec3 up = Cross(right, forward);

  Mat4 result = Identity();
  result.m[0] = right.x;
  result.m[1] = up.x;
  result.m[2] = -forward.x;
  result.m[4] = right.y;
  result.m[5] = up.y;
  result.m[6] = -forward.y;
  result.m[8] = right.z;
  result.m[9] = up.z;
  result.m[10] = -forward.z;
  result.m[12] = -Dot(right, eye);
  result.m[13] = -Dot(up, eye);
  result.m[14] = Dot(forward, eye);
  return result;
}

}  // namespace pointmod
