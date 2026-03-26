#pragma once

#include "AsyncPointCloudLoader.hpp"
#include "OrbitCamera.hpp"
#include "PointCloudRenderer.hpp"

struct GLFWwindow;

namespace pointmod {

class Application {
 public:
  int Run();

 private:
  enum class HideBoxGizmoMode {
    kMove,
    kScale,
    kRotate,
  };

  enum class HideBoxGizmoAxis {
    kNone,
    kX,
    kY,
    kZ,
  };

  struct HideBoxGizmoDragState {
    bool active = false;
    bool dirty = false;
    HideBoxGizmoMode mode = HideBoxGizmoMode::kMove;
    HideBoxGizmoAxis axis = HideBoxGizmoAxis::kNone;
    int boxIndex = -1;
    Vec3 startCenter = {0.0f, 0.0f, 0.0f};
    Vec3 startHalfSize = {0.5f, 0.5f, 0.5f};
    Vec3 startRotationDegrees = {0.0f, 0.0f, 0.0f};
    float startAxisParameter = 0.0f;
    Vec3 startPlaneVector = {1.0f, 0.0f, 0.0f};
  };

  void InitializeWindow();
  void InitializeImGui();
  void Shutdown();
  void BeginImGuiFrame();
  void EndImGuiFrame();
  void RenderUi();
  void RenderScene();
  void UpdateHideBoxGizmo();
  void UpdateCloudLoading();
  void UpdateFrameStats();
  void UpdateInteractionPointBudget();
  void HandleCameraInput();
  void StartOpenDialog();
  void OpenPointCloud(const std::filesystem::path& path);
  void AddHideBox();
  void ClearHideBoxes();
  void CommitHideBoxes();
  void ResetHideBoxGizmo();

  GLFWwindow* window_ = nullptr;
  AsyncPointCloudLoader loader_;
  PointCloudRenderer renderer_;
  OrbitCamera camera_;
  PointCloudData currentCloud_;
  std::string statusMessage_ = "Open a .ply file, drag one into the window, or use the Open button.";
  float pointSize_ = 2.0f;
  float pendingScrollY_ = 0.0f;
  double lastCursorX_ = 0.0;
  double lastCursorY_ = 0.0;
  bool firstCursorSample_ = true;
  bool openShortcutLatched_ = false;
  bool cameraFramed_ = false;
  bool cameraTouched_ = false;
  bool interactionActive_ = false;
  bool interactionTuningActive_ = false;
  double lastFrameTimeSeconds_ = 0.0;
  double lastInteractionTuneTimeSeconds_ = 0.0;
  float smoothedFrameMs_ = 0.0f;
  float smoothedFps_ = 0.0f;
  float interactionPointFraction_ = 1.0f;
  float interactionPointFractionMin_ = 0.0f;
  float interactionPointFractionMax_ = 1.0f;
  std::uint64_t visiblePointCount_ = 0;
  bool visiblePointCountAccurate_ = true;
  std::vector<HideBox> hideBoxes_;
  std::vector<HideBox> committedHideBoxes_;
  bool hideBoxesVisible_ = true;
  int selectedHideBox_ = -1;
  HideBoxGizmoMode hideBoxGizmoMode_ = HideBoxGizmoMode::kMove;
  HideBoxGizmoAxis hideBoxGizmoHotAxis_ = HideBoxGizmoAxis::kNone;
  bool hideBoxGizmoHovered_ = false;
  HideBoxGizmoDragState hideBoxGizmoDrag_;
  RenderDetail activeRenderDetail_ = RenderDetail::kFull;
  bool glfwInitialized_ = false;
  bool imguiInitialized_ = false;
};

}  // namespace pointmod
