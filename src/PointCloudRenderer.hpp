#pragma once

#include "OrbitCamera.hpp"
#include "PointCloud.hpp"

#include <cstddef>
#include <string>

namespace pointmod {

class PointCloudRenderer {
 public:
  ~PointCloudRenderer();

  void Initialize();
  void Shutdown();
  void Upload(const PointCloudData& cloud);
  void Render(const OrbitCamera& camera, int viewportWidth, int viewportHeight, float pointSize) const;

  [[nodiscard]] bool HasCloud() const;
  [[nodiscard]] std::size_t PointCount() const;
  [[nodiscard]] const Bounds& CurrentBounds() const;
  [[nodiscard]] std::string Error() const;

 private:
  unsigned int CompileShader(unsigned int type, const char* source);
  bool initialized_ = false;
  unsigned int program_ = 0;
  unsigned int vao_ = 0;
  unsigned int vbo_ = 0;
  std::size_t pointCount_ = 0;
  Bounds bounds_;
  std::string error_;
};

}  // namespace pointmod
