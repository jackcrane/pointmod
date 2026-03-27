#pragma once

#include "AsyncPointCloudLoader.hpp"
#include "OrbitCamera.hpp"
#include "PerformanceTracker.hpp"
#include "PointCloudRenderer.hpp"

#include <imgui.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <optional>
#include <thread>
#include <unordered_map>

struct GLFWwindow;

namespace pointmod {

class Application {
 public:
  int Run();

 private:
  enum class DeletionWorkflowState {
    kIdle,
    kMarking,
    kConfirmPending,
    kDeleting,
  };

  enum class IsolatedSelectionWorkflowState {
    kIdle,
    kCollecting,
    kSearching,
    kDeleting,
  };

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

  struct PointSelection {
    std::size_t pointIndex = 0;
    float radius = 0.05f;
  };

  struct PointScaleDragState {
    bool active = false;
    int selectionIndex = -1;
    Vec3 planeNormal = {0.0f, 0.0f, 1.0f};
  };

  struct FreeformSelectionState {
    bool pending = false;
    bool active = false;
    bool pendingPointHit = false;
    std::size_t pendingPointIndex = 0;
    ImVec2 pressPosition = {0.0f, 0.0f};
    std::vector<ImVec2> path;
  };

  struct HoverPickCache {
    bool valid = false;
    bool exactResolved = false;
    float mouseX = 0.0f;
    float mouseY = 0.0f;
    Vec3 cameraPosition = {0.0f, 0.0f, 0.0f};
    Vec3 cameraTarget = {0.0f, 0.0f, 0.0f};
    double settleStartSeconds = 0.0;
  };

  struct DeletionGridKey {
    int x = 0;
    int y = 0;
    int z = 0;

    bool operator==(const DeletionGridKey& other) const = default;
  };

  struct DeletionGridKeyHash {
    std::size_t operator()(const DeletionGridKey& key) const;
  };

  struct ExactPointKey {
    std::uint32_t x = 0;
    std::uint32_t y = 0;
    std::uint32_t z = 0;

    bool operator==(const ExactPointKey& other) const = default;
  };

  struct ExactPointKeyHash {
    std::size_t operator()(const ExactPointKey& key) const;
  };

  struct IsolatedSearchWorkerState {
    bool running = false;
    bool completed = false;
    bool hasError = false;
    IsolatedSelectionWorkflowState phase = IsolatedSelectionWorkflowState::kIdle;
    std::size_t processed = 0;
    std::size_t total = 0;
    std::size_t visiblePointCount = 0;
    std::size_t matchedCount = 0;
    std::string message;
    std::vector<std::uint32_t> completedMatches;
  };

  struct SaveProgressState {
    bool active = false;
    std::uint64_t processed = 0;
    std::uint64_t total = 0;
    std::string message;
  };

  void InitializeWindow();
  void InitializeImGui();
  void Shutdown();
  void BeginImGuiFrame();
  void EndImGuiFrame();
  float RenderMenuBar();
  void RenderUi();
  void RenderTaskManagerWindow();
  void RenderSaveProgressOverlay();
  void RenderSelectIsolatedDialog();
  void RenderScene();
  void UpdateTaskManagerMemoryStats();
  void UpdateHideBoxGizmo();
  void UpdatePointSelectionInteraction();
  void UpdateCloudLoading();
  void UpdateFrameStats();
  void UpdateInteractionPointBudget();
  void HandleCameraInput();
  void HandleDeletionInput();
  void UpdateDeletionWorkflow();
  void StartIsolatedSelectionPreview();
  void StartIsolatedSelectionDeletion();
  void UpdateIsolatedSelectionWorkflow();
  void CancelIsolatedSelectionSearch();
  void StartOpenDialog();
  void StartSaveDialog();
  void ResetView();
  void OpenPointCloud(const std::filesystem::path& path);
  void ExportPointCloud(const std::filesystem::path& path);
  void UpdateSaveProgress(std::string message, std::uint64_t processed, std::uint64_t total, bool forcePresent = false);
  void PresentSaveProgressFrame(bool forcePresent = false);
  void AddHideBox();
  void ClearHideBoxes();
  void ClearPointSelections();
  void DeletePointSelection(int selectionIndex);
  void ClearIsolatedSelectionPreview();
  void RebuildPointCloudRenderer();
  std::vector<SelectionSphere> BuildSelectionSpheres() const;
  [[nodiscard]] std::uint64_t EstimatePointCloudMemoryBytes() const;
  [[nodiscard]] std::uint64_t EstimateHideBoxMemoryBytes() const;
  [[nodiscard]] std::uint64_t EstimatePointSelectionMemoryBytes() const;
  [[nodiscard]] std::uint64_t EstimateDeletionWorkflowMemoryBytes() const;
  [[nodiscard]] std::uint64_t EstimateIsolatedSelectionMemoryBytes();
  void BeginDeletionMarking();
  void BeginHideBoxDeletionMarking(int hideBoxIndex);
  void BeginFreeformDeletionMarking(
    std::vector<ImVec2>&& polygon,
    const ImVec2& minScreen,
    const ImVec2& maxScreen,
    const Mat4& viewProjection,
    const ImVec2& viewportOrigin,
    const ImVec2& viewportSize,
    bool additive);
  void CancelDeletionWorkflow();
  void ConfirmDeletion();
  void InvalidateDeletionSpatialIndex();
  void EnsureDeletionSpatialIndex();
  std::vector<std::size_t> CollectDeletionCandidateIndices(const std::vector<SelectionSphere>& selectionSpheres);
  std::vector<std::size_t> CollectFreeformDeletionCandidateIndices();
  void CommitHideBoxes();
  void ResetHideBoxGizmo();

  GLFWwindow* window_ = nullptr;
  AsyncPointCloudLoader loader_;
  PointCloudRenderer renderer_;
  OrbitCamera camera_;
  PointCloudData currentCloud_;
  std::optional<std::filesystem::path> pendingExportPath_;
  std::string statusMessage_ = "Open a .ply file, drag one into the window, or use the Open button.";
  float pointSize_ = 2.0f;
  float pendingScrollY_ = 0.0f;
  double lastCursorX_ = 0.0;
  double lastCursorY_ = 0.0;
  bool firstCursorSample_ = true;
  bool openShortcutLatched_ = false;
  bool saveShortcutLatched_ = false;
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
  PointColorMode pointColorMode_ = PointColorMode::kSource;
  std::array<float, kDepthColorCurveSampleCount> depthColorCurve_ = [] {
    std::array<float, kDepthColorCurveSampleCount> curve{};
    for (std::size_t index = 0; index < curve.size(); ++index) {
      const float depth = curve.size() > 1
        ? static_cast<float>(index) / static_cast<float>(curve.size() - 1)
        : 0.0f;
      curve[index] = 1.0f - depth;
    }
    return curve;
  }();
  bool depthCurveDragActive_ = false;
  int depthCurveDragSample_ = -1;
  float depthCurveDragValue_ = 1.0f;
  HideBoxGizmoMode hideBoxGizmoMode_ = HideBoxGizmoMode::kMove;
  HideBoxGizmoAxis hideBoxGizmoHotAxis_ = HideBoxGizmoAxis::kNone;
  bool hideBoxGizmoHovered_ = false;
  HideBoxGizmoDragState hideBoxGizmoDrag_;
  std::vector<PointSelection> pointSelections_;
  int activePointSelection_ = -1;
  bool hoveredPointActive_ = false;
  std::size_t hoveredPointIndex_ = 0;
  HoverPickCache hoverPickCache_;
  bool pointScaleHandleHovered_ = false;
  int pointScaleHandleHotSelection_ = -1;
  PointScaleDragState pointScaleDrag_;
  FreeformSelectionState freeformSelection_;
  int pointContextMenuSelection_ = -1;
  float pointContextMenuMouseX_ = 0.0f;
  float pointContextMenuMouseY_ = 0.0f;
  bool pointContextMenuOpenRequested_ = false;
  bool consumeRightMouseOrbit_ = false;
  bool backspaceLatched_ = false;
  bool escapeLatched_ = false;
  bool deletionConfirmPending_ = false;
  std::size_t deletionMarkedCount_ = 0;
  DeletionWorkflowState deletionWorkflowState_ = DeletionWorkflowState::kIdle;
  std::vector<std::size_t> deletionMarkedPointIndices_;
  std::vector<std::size_t> deletionCandidateIndices_;
  std::vector<SelectionSphere> deletionSelectionSpheres_;
  HideBox deletionTargetHideBox_{};
  bool deletionTargetHideBoxActive_ = false;
  std::string deletionEmptyMessage_ = "No points matched the deletion selection.";
  std::vector<ImVec2> deletionFreeformPolygon_;
  ImVec2 deletionFreeformMinScreen_ = {0.0f, 0.0f};
  ImVec2 deletionFreeformMaxScreen_ = {0.0f, 0.0f};
  Mat4 deletionFreeformViewProjection_{};
  ImVec2 deletionFreeformViewportOrigin_ = {0.0f, 0.0f};
  ImVec2 deletionFreeformViewportSize_ = {0.0f, 0.0f};
  std::vector<HideBox> deletionFreeformHideBoxes_;
  Vec3 deletionFreeformOrigin_ = {0.0f, 0.0f, 0.0f};
  Vec3 deletionFreeformForward_ = {0.0f, 0.0f, -1.0f};
  std::vector<std::array<Vec3, 3>> deletionFreeformTriangles_;
  bool deletionGridValid_ = false;
  float deletionGridCellSize_ = 1.0f;
  std::unordered_map<DeletionGridKey, std::vector<std::size_t>, DeletionGridKeyHash> deletionGrid_;
  std::vector<std::uint32_t> deletionCandidateStamp_;
  std::uint32_t deletionCandidateStampValue_ = 1;
  std::size_t deletionProcessCursor_ = 0;
  bool selectIsolatedDialogOpen_ = false;
  float isolatedNeighborDistance_ = 0.1f;
  float isolatedPreviewDistance_ = 0.1f;
  float isolatedCellSize_ = 0.0f;
  bool isolatedPreviewValid_ = false;
  std::size_t isolatedMatchedCount_ = 0;
  IsolatedSelectionWorkflowState isolatedSelectionWorkflowState_ = IsolatedSelectionWorkflowState::kIdle;
  std::vector<std::uint32_t> isolatedMatchedPointIndices_;
  std::size_t isolatedVisiblePointCount_ = 0;
  std::size_t isolatedProcessCursor_ = 0;
  SaveProgressState saveProgress_;
  double lastSaveProgressPresentSeconds_ = 0.0;
  std::mutex isolatedSearchWorkerMutex_;
  IsolatedSearchWorkerState isolatedSearchWorkerState_;
  std::jthread isolatedSearchWorker_;
  bool useImGuiMenuBar_ = false;
  bool taskManagerOpen_ = false;
  RenderDetail activeRenderDetail_ = RenderDetail::kFull;
  PerformanceTracker performanceTracker_;
  bool glfwInitialized_ = false;
  bool imguiInitialized_ = false;
};

}  // namespace pointmod
