#pragma once

#include "OrbitCamera.hpp"
#include "PointCloud.hpp"

#include <array>
#include <cstddef>
#include <string>
#include <vector>

namespace pointmod {

enum class RenderDetail {
  kFull,
  kBalanced,
  kInteraction,
};

enum class PointColorMode {
  kSource = 0,
  kDepth = 1,
};

constexpr std::size_t kDepthColorCurveSampleCount = 32;

class PointCloudRenderer {
 public:
  ~PointCloudRenderer();

  void Initialize();
  void Shutdown();
  void Clear();
  void Append(const PointCloudChunk& chunk);
  void SetPointCloud(const std::vector<PointVertex>& points, const Bounds& bounds);
  void UpdatePointCloud(const std::vector<PointVertex>& points, const Bounds& bounds);
  void SetHideBoxes(const std::vector<HideBox>& hideBoxes);
  void Render(
    const OrbitCamera& camera,
    int viewportWidth,
    int viewportHeight,
    float projectionAspectRatio,
    float pointSize,
    PointColorMode colorMode,
    const std::array<float, kDepthColorCurveSampleCount>& depthCurve,
    RenderDetail detail,
    float interactionPointFraction,
    const std::vector<HideBox>& displayHideBoxes,
    bool drawHideBoxes,
    int selectedHideBox,
    const std::vector<SelectionSphere>& selectionSpheres,
    bool drawHoveredPoint,
    const Vec3& hoveredPoint) const;

  [[nodiscard]] bool HasCloud() const;
  [[nodiscard]] std::size_t PointCount() const;
  [[nodiscard]] std::size_t DisplayPointCount(RenderDetail detail, float interactionPointFraction) const;
  [[nodiscard]] std::uint64_t ApproximateGpuBytes() const;
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
  void RenderSelectionOverlay(
    const Mat4& viewProjection,
    float pointSize,
    const std::vector<SelectionSphere>& selectionSpheres,
    bool drawHoveredPoint,
    const Vec3& hoveredPoint) const;
  bool initialized_ = false;
  unsigned int program_ = 0;
  int viewProjectionLocation_ = -1;
  int pointSizeLocation_ = -1;
  int colorModeLocation_ = -1;
  int cameraPositionLocation_ = -1;
  int depthRangeLocation_ = -1;
  int depthCurveLocation_ = -1;
  int hideBoxCountLocation_ = -1;
  int hideBoxTextureLocation_ = -1;
  unsigned int hideBoxTexture_ = 0;
  int hideBoxCount_ = 0;
  unsigned int overlayVao_ = 0;
  unsigned int overlayVbo_ = 0;
  std::vector<GpuChunk> chunks_;
  std::size_t pointCount_ = 0;
  Bounds bounds_;
  std::string error_;
};

}  // namespace pointmod
