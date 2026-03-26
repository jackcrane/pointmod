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

struct Quaternion {
  float x = 0.0f;
  float y = 0.0f;
  float z = 0.0f;
  float w = 1.0f;
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

inline float DegreesToRadians(float degrees) {
  return degrees * 0.01745329252f;
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

inline Mat4 Translation(const Vec3& value) {
  Mat4 result = Identity();
  result.m[12] = value.x;
  result.m[13] = value.y;
  result.m[14] = value.z;
  return result;
}

inline Mat4 RotationX(float radians) {
  Mat4 result = Identity();
  const float cosine = std::cos(radians);
  const float sine = std::sin(radians);
  result.m[5] = cosine;
  result.m[6] = sine;
  result.m[9] = -sine;
  result.m[10] = cosine;
  return result;
}

inline Mat4 RotationY(float radians) {
  Mat4 result = Identity();
  const float cosine = std::cos(radians);
  const float sine = std::sin(radians);
  result.m[0] = cosine;
  result.m[2] = -sine;
  result.m[8] = sine;
  result.m[10] = cosine;
  return result;
}

inline Mat4 RotationZ(float radians) {
  Mat4 result = Identity();
  const float cosine = std::cos(radians);
  const float sine = std::sin(radians);
  result.m[0] = cosine;
  result.m[1] = sine;
  result.m[4] = -sine;
  result.m[5] = cosine;
  return result;
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

inline Mat4 EulerRotationXYZ(const Vec3& degrees) {
  return Multiply(
    RotationZ(DegreesToRadians(degrees.z)),
    Multiply(
      RotationY(DegreesToRadians(degrees.y)),
      RotationX(DegreesToRadians(degrees.x))));
}

inline Quaternion Normalize(const Quaternion& value) {
  const float length = std::sqrt(value.x * value.x + value.y * value.y + value.z * value.z + value.w * value.w);
  if (length <= 0.0f) {
    return {};
  }
  return {value.x / length, value.y / length, value.z / length, value.w / length};
}

inline Quaternion QuaternionFromEulerXYZ(const Vec3& degrees) {
  const float halfX = DegreesToRadians(degrees.x) * 0.5f;
  const float halfY = DegreesToRadians(degrees.y) * 0.5f;
  const float halfZ = DegreesToRadians(degrees.z) * 0.5f;

  const float cx = std::cos(halfX);
  const float sx = std::sin(halfX);
  const float cy = std::cos(halfY);
  const float sy = std::sin(halfY);
  const float cz = std::cos(halfZ);
  const float sz = std::sin(halfZ);

  return Normalize({
    sx * cy * cz - cx * sy * sz,
    cx * sy * cz + sx * cy * sz,
    cx * cy * sz - sx * sy * cz,
    cx * cy * cz + sx * sy * sz,
  });
}

inline Vec3 TransformVector(const Mat4& matrix, const Vec3& value) {
  return {
    matrix.m[0] * value.x + matrix.m[4] * value.y + matrix.m[8] * value.z,
    matrix.m[1] * value.x + matrix.m[5] * value.y + matrix.m[9] * value.z,
    matrix.m[2] * value.x + matrix.m[6] * value.y + matrix.m[10] * value.z,
  };
}

inline Vec3 TransformPoint(const Mat4& matrix, const Vec3& value) {
  return {
    matrix.m[0] * value.x + matrix.m[4] * value.y + matrix.m[8] * value.z + matrix.m[12],
    matrix.m[1] * value.x + matrix.m[5] * value.y + matrix.m[9] * value.z + matrix.m[13],
    matrix.m[2] * value.x + matrix.m[6] * value.y + matrix.m[10] * value.z + matrix.m[14],
  };
}

inline Vec3 InverseRotateVector(const Mat4& rotation, const Vec3& value) {
  return {
    rotation.m[0] * value.x + rotation.m[1] * value.y + rotation.m[2] * value.z,
    rotation.m[4] * value.x + rotation.m[5] * value.y + rotation.m[6] * value.z,
    rotation.m[8] * value.x + rotation.m[9] * value.y + rotation.m[10] * value.z,
  };
}

inline bool Contains(const HideBox& box, const Vec3& point) {
  const Mat4 rotation = EulerRotationXYZ(box.rotationDegrees);
  const Vec3 local = InverseRotateVector(rotation, point - box.center);
  return
    std::abs(local.x) <= box.halfSize.x &&
    std::abs(local.y) <= box.halfSize.y &&
    std::abs(local.z) <= box.halfSize.z;
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
