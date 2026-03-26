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
  void InitializeWindow();
  void InitializeImGui();
  void Shutdown();
  void BeginImGuiFrame();
  void EndImGuiFrame();
  void RenderUi();
  void RenderScene();
  void UpdateCloudLoading();
  void UpdateFrameStats();
  void HandleCameraInput();
  void StartOpenDialog();
  void OpenPointCloud(const std::filesystem::path& path);
  void AddHideBox();
  void ClearHideBoxes();
  void RebuildVisiblePointCloud();
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
  double lastFrameTimeSeconds_ = 0.0;
  float smoothedFrameMs_ = 0.0f;
  float smoothedFps_ = 0.0f;
  std::uint64_t visiblePointCount_ = 0;
  std::vector<HideBox> hideBoxes_;
  bool hideBoxesVisible_ = true;
  int selectedHideBox_ = -1;
  Vec3 hideBoxMoveGizmo_ = {0.0f, 0.0f, 0.0f};
  RenderDetail activeRenderDetail_ = RenderDetail::kFull;
  bool glfwInitialized_ = false;
  bool imguiInitialized_ = false;
};

}  // namespace pointmod
