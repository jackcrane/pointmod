#pragma once

#include <cstdint>
#include <filesystem>
#include <limits>
#include <vector>

namespace pointmod {

struct Vec3 {
  float x = 0.0f;
  float y = 0.0f;
  float z = 0.0f;
};

struct PointVertex {
  float x = 0.0f;
  float y = 0.0f;
  float z = 0.0f;
  std::uint8_t r = 255;
  std::uint8_t g = 255;
  std::uint8_t b = 255;
  std::uint8_t a = 255;
};

struct HideBox {
  Vec3 center = {0.0f, 0.0f, 0.0f};
  Vec3 rotationDegrees = {0.0f, 0.0f, 0.0f};
  Vec3 halfSize = {0.5f, 0.5f, 0.5f};
};

struct SelectionSphere {
  Vec3 center = {0.0f, 0.0f, 0.0f};
  float radius = 2.0f;
};

struct Bounds {
  Vec3 min = {
    std::numeric_limits<float>::max(),
    std::numeric_limits<float>::max(),
    std::numeric_limits<float>::max(),
  };
  Vec3 max = {
    std::numeric_limits<float>::lowest(),
    std::numeric_limits<float>::lowest(),
    std::numeric_limits<float>::lowest(),
  };

  void Expand(const Vec3& value);
  [[nodiscard]] bool IsValid() const;
  [[nodiscard]] Vec3 Center() const;
  [[nodiscard]] Vec3 Extents() const;
  [[nodiscard]] float Radius() const;
};

struct PointCloudChunk {
  std::vector<PointVertex> points;
  Bounds bounds;
  std::uint64_t sourcePointsRead = 0;
  std::uint64_t sourcePointCount = 0;
  std::uint64_t renderedPointCount = 0;
  bool sampledRender = false;
  std::uint64_t samplingStride = 1;
};

struct PointCloudData {
  std::filesystem::path sourcePath;
  std::vector<PointVertex> points;
  Bounds bounds;
  std::uint64_t sourcePointCount = 0;
  std::uint64_t renderPointCount = 0;
  bool sampledRender = false;
  std::uint64_t samplingStride = 1;
};

}  // namespace pointmod
