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
  void Render(const OrbitCamera& camera, int viewportWidth, int viewportHeight, float pointSize, RenderDetail detail) const;

  [[nodiscard]] bool HasCloud() const;
  [[nodiscard]] std::size_t PointCount() const;
  [[nodiscard]] std::size_t DisplayPointCount(RenderDetail detail) const;
  [[nodiscard]] const Bounds& CurrentBounds() const;
  [[nodiscard]] std::string Error() const;

 private:
  struct GpuChunk {
    unsigned int vao = 0;
    unsigned int vbo = 0;
    std::size_t pointCount = 0;
    unsigned int balancedVao = 0;
    unsigned int balancedVbo = 0;
    std::size_t balancedPointCount = 0;
    unsigned int interactionVao = 0;
    unsigned int interactionVbo = 0;
    std::size_t interactionPointCount = 0;
  };

  unsigned int CompileShader(unsigned int type, const char* source);
  bool initialized_ = false;
  unsigned int program_ = 0;
  int viewProjectionLocation_ = -1;
  int pointSizeLocation_ = -1;
  std::vector<GpuChunk> chunks_;
  std::size_t pointCount_ = 0;
  Bounds bounds_;
  std::string error_;
};

}  // namespace pointmod
