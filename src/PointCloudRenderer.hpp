#pragma once

#include "OrbitCamera.hpp"
#include "PointCloud.hpp"

#include <cstddef>
#include <string>
#include <vector>

namespace pointmod {

enum class RenderDetail {
  kFull,
  kBalanced,
  kInteraction,
};

class PointCloudRenderer {
 public:
  ~PointCloudRenderer();

  void Initialize();
  void Shutdown();
  void Clear();
  void Append(const PointCloudChunk& chunk);
  void SetPointCloud(const std::vector<PointVertex>& points, const Bounds& bounds);
  void Render(
    const OrbitCamera& camera,
    int viewportWidth,
    int viewportHeight,
    float pointSize,
    RenderDetail detail,
    float interactionPointFraction,
    const std::vector<HideBox>& hideBoxes,
    bool drawHideBoxes,
    int selectedHideBox) const;

  [[nodiscard]] bool HasCloud() const;
  [[nodiscard]] std::size_t PointCount() const;
  [[nodiscard]] std::size_t DisplayPointCount(RenderDetail detail, float interactionPointFraction) const;
  [[nodiscard]] const Bounds& CurrentBounds() const;
  [[nodiscard]] std::string Error() const;

 private:
  struct GpuChunk {
    unsigned int vao = 0;
    unsigned int vbo = 0;
    std::size_t pointCount = 0;
  };

  unsigned int CompileShader(unsigned int type, const char* source);
  void RenderHideBoxes(
    const Mat4& viewProjection,
    const std::vector<HideBox>& hideBoxes,
    int selectedHideBox) const;
  bool initialized_ = false;
  unsigned int program_ = 0;
  int viewProjectionLocation_ = -1;
  int pointSizeLocation_ = -1;
  unsigned int overlayVao_ = 0;
  unsigned int overlayVbo_ = 0;
  std::vector<GpuChunk> chunks_;
  std::size_t pointCount_ = 0;
  Bounds bounds_;
  std::string error_;
};

}  // namespace pointmod
