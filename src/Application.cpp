#define GLFW_INCLUDE_NONE
#include "Application.hpp"

#include "CocoaMenu.hpp"
#include "FileDialog.hpp"
#include "OpenGL.hpp"

#include <GLFW/glfw3.h>
#include <imgui.h>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_opengl3.h>

#include <algorithm>
#include <bit>
#include <cfloat>
#include <stdexcept>
#include <string>
#include <vector>

namespace pointmod {

namespace {

constexpr float kGizmoMinHalfSize = 0.001f;
constexpr float kGizmoHitRadiusPixels = 12.0f;
constexpr float kGizmoArrowHeadPixels = 12.0f;
constexpr float kGizmoRotateRadiusFactor = 0.85f;
constexpr int kGizmoCircleSegments = 48;
constexpr float kPointHoverPaddingPixels = 12.0f;
constexpr float kPointClickPaddingPixels = 14.0f;
constexpr float kPointScaleHandleRadiusPixels = 10.0f;
constexpr float kPointSelectionDefaultRadius = 0.05f;
constexpr float kPointSelectionMinRadius = 0.0025f;
constexpr std::size_t kHoverPickTargetPoints = 120'000;
constexpr double kHoverExactSettleDelaySeconds = 0.08;
constexpr float kHoverRequeryDistancePixels = 2.0f;
constexpr float kPickDepthPreferenceSlack = 0.2f;
constexpr float kPointContextHitPaddingPixels = 6.0f;
constexpr float kRadiansToDegrees = 57.2957795131f;
constexpr float kRotationDragPlaneEpsilon = 0.0001f;
constexpr float kInteractionTargetFpsMin = 45.0f;
constexpr float kInteractionTargetFpsMax = 60.0f;
constexpr float kInteractionProbeFps = 50.0f;
constexpr float kInteractionPointDropFactor = 0.5f;
constexpr float kMinInteractionPointFraction = 0.001f;
constexpr double kInteractionTuneIntervalSeconds = 0.2;
constexpr std::size_t kDeletionWorkChunkPoints = 200'000;
constexpr std::size_t kIsolatedWorkChunkPoints = 100'000;
constexpr std::size_t kIsolatedWorkChunkCells = 256;
constexpr Vec3 kWorldUp = {0.0f, 0.0f, 1.0f};
constexpr std::uint8_t kPointFlagIsolatedPreview = 1 << 0;
constexpr std::uint8_t kPointFlagMarkedForDeletion = 1 << 1;

struct CameraRay {
  Vec3 origin = {0.0f, 0.0f, 0.0f};
  Vec3 direction = {0.0f, 0.0f, -1.0f};
};

struct PointPickResult {
  bool hit = false;
  std::size_t pointIndex = 0;
  float screenDistanceSquared = FLT_MAX;
  float cameraDistance = FLT_MAX;
};

Vec3 PointPosition(const PointVertex& point) {
  return {point.x, point.y, point.z};
}

float GetAxisComponent(const Vec3& value, int axisIndex) {
  switch (axisIndex) {
    case 0:
      return value.x;
    case 1:
      return value.y;
    default:
      return value.z;
  }
}

void SetAxisComponent(Vec3& value, int axisIndex, float component) {
  switch (axisIndex) {
    case 0:
      value.x = component;
      break;
    case 1:
      value.y = component;
      break;
    default:
      value.z = component;
      break;
  }
}

Vec3 UnitAxis(int axisIndex) {
  switch (axisIndex) {
    case 0:
      return {1.0f, 0.0f, 0.0f};
    case 1:
      return {0.0f, 1.0f, 0.0f};
    default:
      return {0.0f, 0.0f, 1.0f};
  }
}

ImU32 AxisColor(int axisIndex, bool highlighted) {
  switch (axisIndex) {
    case 0:
      return highlighted ? IM_COL32(255, 138, 128, 255) : IM_COL32(229, 57, 53, 255);
    case 1:
      return highlighted ? IM_COL32(165, 214, 167, 255) : IM_COL32(67, 160, 71, 255);
    default:
      return highlighted ? IM_COL32(144, 202, 249, 255) : IM_COL32(30, 136, 229, 255);
  }
}

ImVec2 Add(const ImVec2& a, const ImVec2& b) {
  return {a.x + b.x, a.y + b.y};
}

ImVec2 Subtract(const ImVec2& a, const ImVec2& b) {
  return {a.x - b.x, a.y - b.y};
}

ImVec2 Multiply(const ImVec2& value, float scalar) {
  return {value.x * scalar, value.y * scalar};
}

float LengthSquared(const ImVec2& value) {
  return value.x * value.x + value.y * value.y;
}

float DistanceSquared(const ImVec2& a, const ImVec2& b) {
  return LengthSquared(Subtract(a, b));
}

float DistanceSquared(const Vec3& a, const Vec3& b) {
  const Vec3 delta = a - b;
  return Dot(delta, delta);
}

float Clamp01(float value) {
  return (std::clamp)(value, 0.0f, 1.0f);
}

ImVec2 Normalize2D(const ImVec2& value) {
  const float lengthSquared = LengthSquared(value);
  if (lengthSquared <= 0.0f) {
    return {1.0f, 0.0f};
  }
  const float inverseLength = 1.0f / std::sqrt(lengthSquared);
  return {value.x * inverseLength, value.y * inverseLength};
}

float DistanceToSegmentSquared(const ImVec2& point, const ImVec2& start, const ImVec2& end) {
  const ImVec2 segment = Subtract(end, start);
  const float segmentLengthSquared = LengthSquared(segment);
  if (segmentLengthSquared <= 0.0f) {
    return LengthSquared(Subtract(point, start));
  }

  const ImVec2 offset = Subtract(point, start);
  const float t = (std::clamp)((offset.x * segment.x + offset.y * segment.y) / segmentLengthSquared, 0.0f, 1.0f);
  const ImVec2 closest = Add(start, Multiply(segment, t));
  return LengthSquared(Subtract(point, closest));
}

bool ProjectToScreen(
  const Vec3& point,
  const Mat4& viewProjection,
  const ImVec2& viewportOrigin,
  const ImVec2& viewportSize,
  ImVec2& screenPoint) {
  const float clipX = viewProjection.m[0] * point.x + viewProjection.m[4] * point.y + viewProjection.m[8] * point.z + viewProjection.m[12];
  const float clipY = viewProjection.m[1] * point.x + viewProjection.m[5] * point.y + viewProjection.m[9] * point.z + viewProjection.m[13];
  const float clipW = viewProjection.m[3] * point.x + viewProjection.m[7] * point.y + viewProjection.m[11] * point.z + viewProjection.m[15];
  if (clipW <= 0.0001f) {
    return false;
  }

  const float ndcX = clipX / clipW;
  const float ndcY = clipY / clipW;
  screenPoint = {
    viewportOrigin.x + (ndcX * 0.5f + 0.5f) * viewportSize.x,
    viewportOrigin.y + (1.0f - (ndcY * 0.5f + 0.5f)) * viewportSize.y,
  };
  return true;
}

CameraRay BuildCameraRay(
  const OrbitCamera& camera,
  float mouseX,
  float mouseY,
  float viewportWidth,
  float viewportHeight) {
  const float aspectRatio = viewportHeight > 0.0f ? viewportWidth / viewportHeight : 1.0f;
  const float ndcX = viewportWidth > 0.0f ? mouseX / viewportWidth * 2.0f - 1.0f : 0.0f;
  const float ndcY = viewportHeight > 0.0f ? 1.0f - mouseY / viewportHeight * 2.0f : 0.0f;
  const Vec3 eye = camera.Position();
  const Vec3 forward = Normalize(camera.Target() - eye);
  Vec3 right = Normalize(Cross(forward, kWorldUp));
  if (Length(right) <= 0.0f) {
    right = {1.0f, 0.0f, 0.0f};
  }
  const Vec3 up = Normalize(Cross(right, forward));
  const float tanHalfFov = std::tan(0.85f * 0.5f);
  return CameraRay{
    .origin = eye,
    .direction = Normalize(
      forward +
      right * (ndcX * tanHalfFov * aspectRatio) +
      up * (ndcY * tanHalfFov)),
  };
}

void BuildCameraBasis(const OrbitCamera& camera, Vec3& forward, Vec3& right, Vec3& up) {
  const Vec3 eye = camera.Position();
  forward = Normalize(camera.Target() - eye);
  right = Normalize(Cross(forward, kWorldUp));
  if (Length(right) <= 0.0f) {
    right = {1.0f, 0.0f, 0.0f};
  }
  up = Normalize(Cross(right, forward));
}

bool IntersectRayPlane(const CameraRay& ray, const Vec3& planeOrigin, const Vec3& planeNormal, Vec3& hitPoint) {
  const float denominator = Dot(ray.direction, planeNormal);
  if (std::abs(denominator) <= 0.00001f) {
    return false;
  }

  const float distance = Dot(planeOrigin - ray.origin, planeNormal) / denominator;
  if (distance < 0.0f) {
    return false;
  }

  hitPoint = ray.origin + ray.direction * distance;
  return true;
}

Vec3 BuildPlaneNormalForAxisDrag(const Vec3& axis, const Vec3& eyeToOrigin) {
  Vec3 planeNormal = Cross(Cross(axis, eyeToOrigin), axis);
  if (Length(planeNormal) <= 0.00001f) {
    planeNormal = Cross(Cross(axis, kWorldUp), axis);
  }
  if (Length(planeNormal) <= 0.00001f) {
    planeNormal = Cross(Cross(axis, Vec3{0.0f, 1.0f, 0.0f}), axis);
  }
  return Normalize(planeNormal);
}

void BuildPlaneBasis(const Vec3& normal, Vec3& tangent, Vec3& bitangent) {
  tangent = Normalize(Cross(std::abs(normal.z) < 0.95f ? kWorldUp : Vec3{0.0f, 1.0f, 0.0f}, normal));
  if (Length(tangent) <= 0.00001f) {
    tangent = {1.0f, 0.0f, 0.0f};
  }
  bitangent = Normalize(Cross(normal, tangent));
}

float SignedAngleRadians(const Vec3& from, const Vec3& to, const Vec3& axis) {
  return std::atan2(Dot(axis, Cross(from, to)), Dot(from, to));
}

Vec3 ClampHalfSize(const Vec3& halfSize) {
  return {
    (std::max)(halfSize.x, kGizmoMinHalfSize),
    (std::max)(halfSize.y, kGizmoMinHalfSize),
    (std::max)(halfSize.z, kGizmoMinHalfSize),
  };
}

bool IsPointHidden(const Vec3& point, const std::vector<HideBox>& hideBoxes) {
  for (const HideBox& hideBox : hideBoxes) {
    if (Contains(hideBox, point)) {
      return true;
    }
  }
  return false;
}

bool IsInsideSphere(const Vec3& point, const SelectionSphere& sphere) {
  const float radius = (std::max)(sphere.radius, 0.0f);
  return Length(point - sphere.center) <= radius;
}

bool IsInsideConnector(const Vec3& point, const SelectionSphere& start, const SelectionSphere& end) {
  const Vec3 axis = end.center - start.center;
  const float axisLengthSquared = Dot(axis, axis);
  if (axisLengthSquared <= 0.0000001f) {
    return false;
  }

  const float t = Clamp01(Dot(point - start.center, axis) / axisLengthSquared);
  const Vec3 closestPoint = start.center + axis * t;
  const float radius = start.radius + (end.radius - start.radius) * t;
  return Length(point - closestPoint) <= (std::max)(radius, 0.0f);
}

Bounds ComputeBounds(const std::vector<PointVertex>& points) {
  Bounds bounds;
  for (const PointVertex& point : points) {
    bounds.Expand(PointPosition(point));
  }
  return bounds;
}

float ComputePickRadiusPixels(float pointSize, float paddingPixels) {
  return (std::max)(pointSize * 0.75f + paddingPixels, paddingPixels);
}

std::size_t HoverPickStep(std::size_t pointCount) {
  if (pointCount <= kHoverPickTargetPoints) {
    return 1;
  }

  return (pointCount + kHoverPickTargetPoints - 1) / kHoverPickTargetPoints;
}

bool NearlyEqual(float a, float b, float epsilon = 0.0001f) {
  return std::abs(a - b) <= epsilon;
}

bool NearlyEqual(const Vec3& a, const Vec3& b, float epsilon = 0.0001f) {
  return
    NearlyEqual(a.x, b.x, epsilon) &&
    NearlyEqual(a.y, b.y, epsilon) &&
    NearlyEqual(a.z, b.z, epsilon);
}

Vec3 BuildScaleHandleDirection(const OrbitCamera& camera) {
  Vec3 forward;
  Vec3 right;
  Vec3 up;
  BuildCameraBasis(camera, forward, right, up);

  Vec3 direction = Normalize(right + up);
  if (Length(direction) <= 0.0f) {
    direction = right;
  }
  if (Length(direction) <= 0.0f) {
    direction = {1.0f, 0.0f, 0.0f};
  }
  return direction;
}

PointPickResult PickPoint(
  const std::vector<PointVertex>& points,
  const OrbitCamera& camera,
  const Mat4& viewProjection,
  const ImVec2& viewportOrigin,
  const ImVec2& viewportSize,
  const ImVec2& mousePosition,
  float hitRadiusPixels,
  const std::vector<HideBox>& hideBoxes,
  std::size_t pointStep) {
  PointPickResult bestPick;
  if (points.empty() || pointStep == 0) {
    return bestPick;
  }

  const float maxDistanceSquared = hitRadiusPixels * hitRadiusPixels;
  const float depthPreferenceSlack = maxDistanceSquared * kPickDepthPreferenceSlack;
  const Vec3 eye = camera.Position();
  for (std::size_t pointIndex = 0; pointIndex < points.size(); pointIndex += pointStep) {
    const Vec3 position = PointPosition(points[pointIndex]);
    if (IsPointHidden(position, hideBoxes)) {
      continue;
    }

    ImVec2 screenPoint;
    if (!ProjectToScreen(position, viewProjection, viewportOrigin, viewportSize, screenPoint)) {
      continue;
    }

    const float screenDistanceSquared = DistanceSquared(mousePosition, screenPoint);
    if (screenDistanceSquared > maxDistanceSquared) {
      continue;
    }

    const float cameraDistance = Length(position - eye);
    if (
      !bestPick.hit ||
      screenDistanceSquared + depthPreferenceSlack < bestPick.screenDistanceSquared ||
      (std::abs(screenDistanceSquared - bestPick.screenDistanceSquared) <= depthPreferenceSlack && cameraDistance < bestPick.cameraDistance)) {
      bestPick.hit = true;
      bestPick.pointIndex = pointIndex;
      bestPick.screenDistanceSquared = screenDistanceSquared;
      bestPick.cameraDistance = cameraDistance;
    }
  }

  return bestPick;
}

template <std::size_t SampleCount>
void ResetDepthCurve(std::array<float, SampleCount>& curve) {
  for (std::size_t index = 0; index < curve.size(); ++index) {
    const float depth = curve.size() > 1
      ? static_cast<float>(index) / static_cast<float>(curve.size() - 1)
      : 0.0f;
    curve[index] = 1.0f - depth;
  }
}

template <std::size_t SampleCount>
void ApplyDepthCurveStroke(
  std::array<float, SampleCount>& curve,
  int fromSample,
  float fromValue,
  int toSample,
  float toValue) {
  if (curve.empty() || fromSample < 0 || toSample < 0) {
    return;
  }

  fromSample = (std::clamp)(fromSample, 0, static_cast<int>(curve.size()) - 1);
  toSample = (std::clamp)(toSample, 0, static_cast<int>(curve.size()) - 1);
  fromValue = (std::clamp)(fromValue, 0.0f, 1.0f);
  toValue = (std::clamp)(toValue, 0.0f, 1.0f);

  if (fromSample == toSample) {
    curve[static_cast<std::size_t>(toSample)] = toValue;
    return;
  }

  int startSample = fromSample;
  int endSample = toSample;
  float startValue = fromValue;
  float endValue = toValue;
  if (startSample > endSample) {
    std::swap(startSample, endSample);
    std::swap(startValue, endValue);
  }

  for (int sample = startSample; sample <= endSample; ++sample) {
    const float t = endSample > startSample
      ? static_cast<float>(sample - startSample) / static_cast<float>(endSample - startSample)
      : 0.0f;
    curve[static_cast<std::size_t>(sample)] = startValue + (endValue - startValue) * t;
  }
}

template <std::size_t SampleCount>
bool DrawDepthCurveEditor(
  const char* id,
  std::array<float, SampleCount>& curve,
  bool& dragActive,
  int& dragSample,
  float& dragValue) {
  const float width = (std::max)(ImGui::GetContentRegionAvail().x, 160.0f);
  const ImVec2 canvasSize = {width, 170.0f};
  const ImVec2 canvasOrigin = ImGui::GetCursorScreenPos();
  const ImVec2 canvasMax = {canvasOrigin.x + canvasSize.x, canvasOrigin.y + canvasSize.y};
  const ImVec2 graphPadding = {12.0f, 12.0f};
  const ImVec2 graphMin = Add(canvasOrigin, graphPadding);
  const ImVec2 graphMax = Subtract(canvasMax, graphPadding);
  const float graphWidth = (std::max)(graphMax.x - graphMin.x, 1.0f);
  const float graphHeight = (std::max)(graphMax.y - graphMin.y, 1.0f);

  ImGui::InvisibleButton(id, canvasSize, ImGuiButtonFlags_MouseButtonLeft);
  const bool hovered = ImGui::IsItemHovered();
  const bool active = ImGui::IsItemActive();
  const ImGuiIO& io = ImGui::GetIO();

  auto mouseToCurve = [&](const ImVec2& mouse, int& sample, float& value) {
    const float normalizedX = (std::clamp)((mouse.x - graphMin.x) / graphWidth, 0.0f, 1.0f);
    const float normalizedY = (std::clamp)((mouse.y - graphMin.y) / graphHeight, 0.0f, 1.0f);
    sample = static_cast<int>(std::round(normalizedX * static_cast<float>(curve.size() - 1)));
    value = 1.0f - normalizedY;
  };

  bool changed = false;
  if (!ImGui::IsMouseDown(0)) {
    dragActive = false;
    dragSample = -1;
  } else if ((hovered && ImGui::IsMouseClicked(0)) || (dragActive && active)) {
    int sample = 0;
    float value = 1.0f;
    mouseToCurve(io.MousePos, sample, value);
    if (!dragActive) {
      dragActive = true;
      dragSample = sample;
      dragValue = value;
    }
    ApplyDepthCurveStroke(curve, dragSample, dragValue, sample, value);
    dragSample = sample;
    dragValue = value;
    changed = true;
  }

  ImDrawList* drawList = ImGui::GetWindowDrawList();
  drawList->AddRectFilled(canvasOrigin, canvasMax, IM_COL32(18, 21, 26, 230), 6.0f);
  drawList->AddRect(canvasOrigin, canvasMax, hovered || active ? IM_COL32(155, 168, 186, 255) : IM_COL32(82, 91, 105, 255), 6.0f);

  for (int gridIndex = 1; gridIndex < 4; ++gridIndex) {
    const float x = graphMin.x + graphWidth * static_cast<float>(gridIndex) / 4.0f;
    const float y = graphMin.y + graphHeight * static_cast<float>(gridIndex) / 4.0f;
    drawList->AddLine({x, graphMin.y}, {x, graphMax.y}, IM_COL32(52, 60, 72, 255), 1.0f);
    drawList->AddLine({graphMin.x, y}, {graphMax.x, y}, IM_COL32(52, 60, 72, 255), 1.0f);
  }

  std::array<ImVec2, SampleCount> points{};
  for (std::size_t index = 0; index < curve.size(); ++index) {
    const float depth = curve.size() > 1
      ? static_cast<float>(index) / static_cast<float>(curve.size() - 1)
      : 0.0f;
    points[index] = {
      graphMin.x + graphWidth * depth,
      graphMin.y + graphHeight * (1.0f - (std::clamp)(curve[index], 0.0f, 1.0f)),
    };
  }

  drawList->AddPolyline(points.data(), static_cast<int>(points.size()), IM_COL32(236, 239, 244, 255), 0, 2.5f);
  for (const ImVec2& point : points) {
    drawList->AddCircleFilled(point, 2.5f, IM_COL32(236, 239, 244, 255));
  }

  drawList->AddText({graphMin.x, canvasOrigin.y + 2.0f}, IM_COL32(140, 147, 160, 255), "Bright");
  drawList->AddText({graphMin.x, canvasMax.y - 18.0f}, IM_COL32(140, 147, 160, 255), "Near");
  const char* farLabel = "Far";
  const ImVec2 farLabelSize = ImGui::CalcTextSize(farLabel);
  drawList->AddText({graphMax.x - farLabelSize.x, canvasMax.y - 18.0f}, IM_COL32(140, 147, 160, 255), farLabel);

  return changed;
}

const char* PointColorModeLabel(PointColorMode mode) {
  switch (mode) {
    case PointColorMode::kSource:
      return "Source color";
    case PointColorMode::kDepth:
      return "Depth greyscale";
  }
  return "Unknown";
}

HideBox MakeDefaultHideBox(const Bounds& bounds, const OrbitCamera& camera) {
  const Vec3 extents = bounds.Extents();
  const float fallbackSize = (std::max)(bounds.Radius() * 0.2f, 0.1f);
  return HideBox{
    .center = camera.Target(),
    .rotationDegrees = {0.0f, 0.0f, 0.0f},
    .halfSize = ClampHalfSize({
      (std::max)(extents.x * 0.08f, fallbackSize),
      (std::max)(extents.y * 0.08f, fallbackSize),
      (std::max)(extents.z * 0.08f, fallbackSize),
    }),
  };
}

const char* DetailLabel(RenderDetail detail) {
  switch (detail) {
    case RenderDetail::kFull:
      return "Full";
    case RenderDetail::kBalanced:
      return "Balanced";
    case RenderDetail::kInteraction:
      return "Interaction";
  }
  return "Unknown";
}

RenderDetail ChooseRenderDetail(bool interactionActive) {
  if (interactionActive) {
    return RenderDetail::kInteraction;
  }

  return RenderDetail::kFull;
}

}  // namespace

std::size_t Application::DeletionGridKeyHash::operator()(const DeletionGridKey& key) const {
  const std::uint64_t x = static_cast<std::uint32_t>(key.x);
  const std::uint64_t y = static_cast<std::uint32_t>(key.y);
  const std::uint64_t z = static_cast<std::uint32_t>(key.z);
  return static_cast<std::size_t>((x * 73856093ULL) ^ (y * 19349663ULL) ^ (z * 83492791ULL));
}

std::size_t Application::ExactPointKeyHash::operator()(const ExactPointKey& key) const {
  const std::uint64_t x = key.x;
  const std::uint64_t y = key.y;
  const std::uint64_t z = key.z;
  return static_cast<std::size_t>((x * 73856093ULL) ^ (y * 19349663ULL) ^ (z * 83492791ULL));
}

int Application::Run() {
  try {
    InitializeWindow();
    InitializeImGui();
    renderer_.Initialize();

    while (!glfwWindowShouldClose(window_)) {
      glfwPollEvents();
      UpdateCloudLoading();

      BeginImGuiFrame();
      RenderUi();
      UpdateHideBoxGizmo();
      UpdatePointSelectionInteraction();
      HandleCameraInput();
      HandleDeletionInput();
      UpdateDeletionWorkflow();
      UpdateIsolatedSelectionWorkflow();
      UpdateFrameStats();
      RenderScene();
      EndImGuiFrame();
    }

    Shutdown();
    return 0;
  } catch (...) {
    Shutdown();
    throw;
  }
}

void Application::InitializeWindow() {
  if (glfwInit() != GLFW_TRUE) {
    throw std::runtime_error("Failed to initialize GLFW.");
  }
  glfwInitialized_ = true;

  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#if defined(__APPLE__)
  glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);
  glfwWindowHint(GLFW_COCOA_RETINA_FRAMEBUFFER, GLFW_TRUE);
#endif

  window_ = glfwCreateWindow(1600, 1000, "pointmod", nullptr, nullptr);
  if (window_ == nullptr) {
    throw std::runtime_error("Failed to create application window.");
  }

  glfwMakeContextCurrent(window_);
  if (!InitializeOpenGLBindings()) {
    throw std::runtime_error("Failed to load required OpenGL functions.");
  }
  glfwSwapInterval(1);
  glfwSetWindowUserPointer(window_, this);
  glfwSetScrollCallback(window_, [](GLFWwindow* window, double, double yoffset) {
    auto* app = static_cast<Application*>(glfwGetWindowUserPointer(window));
    if (app == nullptr) {
      return;
    }
    app->pendingScrollY_ += static_cast<float>(yoffset);
  });
  glfwSetDropCallback(window_, [](GLFWwindow* window, int pathCount, const char* paths[]) {
    auto* app = static_cast<Application*>(glfwGetWindowUserPointer(window));
    if (app == nullptr || pathCount <= 0 || paths == nullptr) {
      return;
    }
    app->OpenPointCloud(std::filesystem::path(paths[0]));
  });

  const bool nativeMenuInstalled = InstallNativeMenu(
    window_,
    [this]() { StartOpenDialog(); },
    [this]() { ResetView(); });
#if defined(__APPLE__)
  useImGuiMenuBar_ = false;
#else
  useImGuiMenuBar_ = !nativeMenuInstalled;
#endif
}

void Application::InitializeImGui() {
  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGui::StyleColorsDark();

  ImGuiIO& io = ImGui::GetIO();
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

  ImGui_ImplGlfw_InitForOpenGL(window_, true);
  ImGui_ImplOpenGL3_Init("#version 150");
  imguiInitialized_ = true;
}

void Application::Shutdown() {
  if (window_ != nullptr) {
    glfwMakeContextCurrent(window_);
  }
  renderer_.Shutdown();

  if (imguiInitialized_) {
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    imguiInitialized_ = false;
  }

  if (window_ != nullptr) {
    UninstallNativeMenu(window_);
    glfwDestroyWindow(window_);
    window_ = nullptr;
  }

  if (glfwInitialized_) {
    glfwTerminate();
    glfwInitialized_ = false;
  }
}

void Application::BeginImGuiFrame() {
  ImGui_ImplOpenGL3_NewFrame();
  ImGui_ImplGlfw_NewFrame();
  ImGui::NewFrame();
}

void Application::EndImGuiFrame() {
  ImGui::Render();
  ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
  glfwSwapBuffers(window_);
}

float Application::RenderMenuBar() {
  if (!useImGuiMenuBar_) {
    return 0.0f;
  }

  float menuBarHeight = 0.0f;
  if (ImGui::BeginMainMenuBar()) {
    const bool hasLoadedCloud = !currentCloud_.points.empty();
    const bool hasHideBoxes = !hideBoxes_.empty();
    const bool hasPointSelections = !pointSelections_.empty();

    if (ImGui::BeginMenu("File")) {
      if (ImGui::MenuItem("Open...", "Ctrl+O")) {
        StartOpenDialog();
      }
      ImGui::Separator();
      if (ImGui::MenuItem("Quit")) {
        glfwSetWindowShouldClose(window_, GLFW_TRUE);
      }
      ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("View")) {
      if (ImGui::MenuItem("Reset view", nullptr, false, hasLoadedCloud)) {
        ResetView();
      }
      ImGui::MenuItem(hideBoxesVisible_ ? "Hide boxes" : "Show boxes", nullptr, &hideBoxesVisible_, hasHideBoxes);
      ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Selection")) {
      if (ImGui::MenuItem("Add hide box", nullptr, false, hasLoadedCloud)) {
        AddHideBox();
      }
      if (ImGui::MenuItem("Delete all hide boxes", nullptr, false, hasHideBoxes)) {
        ClearHideBoxes();
      }
      if (ImGui::MenuItem("Clear point spheres", nullptr, false, hasPointSelections)) {
        ClearPointSelections();
      }
      ImGui::EndMenu();
    }

    menuBarHeight = ImGui::GetWindowSize().y;
    ImGui::EndMainMenuBar();
  }

  return menuBarHeight;
}

void Application::RenderUi() {
  const float menuBarHeight = RenderMenuBar();
  const ImGuiViewport* viewport = ImGui::GetMainViewport();
  ImGui::SetNextWindowPos(
    ImVec2(viewport->Pos.x + 20.0f, viewport->Pos.y + menuBarHeight + 20.0f),
    ImGuiCond_Always);
  ImGui::SetNextWindowSize(ImVec2(430.0f, 560.0f), ImGuiCond_Always);
  ImGui::SetNextWindowBgAlpha(0.82f);

  ImGuiWindowFlags flags =
    ImGuiWindowFlags_NoMove |
    ImGuiWindowFlags_NoResize |
    ImGuiWindowFlags_NoCollapse;

  if (ImGui::Begin("Viewer", nullptr, flags)) {
    if (ImGui::Button("Open...")) {
      StartOpenDialog();
    }
    ImGui::SameLine();
    const bool hasLoadedCloud = !currentCloud_.points.empty();
    if (!hasLoadedCloud) {
      ImGui::BeginDisabled();
    }
    if (ImGui::Button("Reset view")) {
      ResetView();
    }
    if (!hasLoadedCloud) {
      ImGui::EndDisabled();
    }
    ImGui::SameLine();
    if (!hasLoadedCloud) {
      ImGui::BeginDisabled();
    }
    if (ImGui::Button("Add hide box")) {
      AddHideBox();
    }
    if (!hasLoadedCloud) {
      ImGui::EndDisabled();
    }
    ImGui::SameLine();
#if defined(__APPLE__)
    ImGui::TextUnformatted("Shortcut: Cmd+O or drag and drop");
#else
    ImGui::TextUnformatted("Shortcut: Ctrl+O or drag and drop");
#endif

    const bool hasHideBoxes = !hideBoxes_.empty();
    if (!hasHideBoxes) {
      ImGui::BeginDisabled();
    }
    if (ImGui::Button(hideBoxesVisible_ ? "Hide boxes" : "Show boxes")) {
      hideBoxesVisible_ = !hideBoxesVisible_;
    }
    ImGui::SameLine();
    if (ImGui::Button("Delete all hide boxes")) {
      ClearHideBoxes();
    }
    if (!hasHideBoxes) {
      ImGui::EndDisabled();
    }
    ImGui::SameLine();
    const bool hasPointSelections = !pointSelections_.empty();
    if (!hasPointSelections) {
      ImGui::BeginDisabled();
    }
    if (ImGui::Button("Clear point spheres")) {
      ClearPointSelections();
    }
    if (!hasPointSelections) {
      ImGui::EndDisabled();
    }
    ImGui::SameLine();
    if (!hasLoadedCloud) {
      ImGui::BeginDisabled();
    }
    if (ImGui::Button("Select isolated")) {
      selectIsolatedDialogOpen_ = true;
    }
    if (!hasLoadedCloud) {
      ImGui::EndDisabled();
    }

    ImGui::TextWrapped("%s", statusMessage_.c_str());
    ImGui::Separator();

    if (currentCloud_.renderPointCount > 0) {
      const std::string fileName = currentCloud_.sourcePath.filename().string();
      ImGui::Text("File: %s", fileName.c_str());
      ImGui::Text("Source points: %llu", static_cast<unsigned long long>(currentCloud_.sourcePointCount));
      ImGui::Text("Sampled points: %llu", static_cast<unsigned long long>(currentCloud_.renderPointCount));
      if (visiblePointCountAccurate_) {
        ImGui::Text("Visible points: %llu", static_cast<unsigned long long>(visiblePointCount_));
      } else {
        ImGui::TextUnformatted("Visible points: filtered on GPU");
      }
      ImGui::Text("Sampling: %s", currentCloud_.sampledRender ? "enabled" : "off");
      if (currentCloud_.sampledRender) {
        ImGui::Text("Sampling stride: 1 / %llu", static_cast<unsigned long long>(currentCloud_.samplingStride));
      }
      ImGui::Text("Hide boxes: %llu", static_cast<unsigned long long>(hideBoxes_.size()));
      ImGui::Text("Point spheres: %llu", static_cast<unsigned long long>(pointSelections_.size()));
      if (activePointSelection_ >= 0 && activePointSelection_ < static_cast<int>(pointSelections_.size())) {
        ImGui::Text("Active sphere radius: %.3f", pointSelections_[static_cast<std::size_t>(activePointSelection_)].radius);
      }
      ImGui::Text("FPS: %.1f (%.2f ms)", smoothedFps_, smoothedFrameMs_);
      ImGui::Text(
        "Display detail: %s (%llu pts)",
        DetailLabel(activeRenderDetail_),
        static_cast<unsigned long long>(renderer_.DisplayPointCount(activeRenderDetail_, interactionPointFraction_)));
    } else {
      ImGui::TextUnformatted("No point cloud loaded");
    }

    AsyncPointCloudLoader::State loaderState = loader_.Snapshot();
    if (loaderState.loading) {
      const float progress = loaderState.progress.totalBytes > 0
        ? static_cast<float>(loaderState.progress.bytesRead) / static_cast<float>(loaderState.progress.totalBytes)
        : 0.0f;
      ImGui::ProgressBar(std::clamp(progress, 0.0f, 1.0f), ImVec2(-1.0f, 0.0f), loaderState.message.c_str());
      ImGui::Text("Read: %llu points", static_cast<unsigned long long>(loaderState.progress.pointsRead));
      ImGui::Text("Rendered: %llu points", static_cast<unsigned long long>(loaderState.progress.pointsKept));
    } else if (loaderState.hasError) {
      ImGui::TextColored(ImVec4(1.0f, 0.45f, 0.45f, 1.0f), "%s", loaderState.message.c_str());
    }

    ImGui::SliderFloat("Point size", &pointSize_, 1.0f, 8.0f, "%.1f px");
    ImGui::Text("Coloring: %s", PointColorModeLabel(pointColorMode_));
    if (ImGui::RadioButton("Source color", pointColorMode_ == PointColorMode::kSource)) {
      pointColorMode_ = PointColorMode::kSource;
    }
    ImGui::SameLine();
    if (ImGui::RadioButton("Depth greyscale", pointColorMode_ == PointColorMode::kDepth)) {
      pointColorMode_ = PointColorMode::kDepth;
    }
    if (pointColorMode_ == PointColorMode::kDepth) {
      if (ImGui::Button("Reset depth curve")) {
        ResetDepthCurve(depthColorCurve_);
        depthCurveDragActive_ = false;
        depthCurveDragSample_ = -1;
      }
      DrawDepthCurveEditor(
        "##depth-curve",
        depthColorCurve_,
        depthCurveDragActive_,
        depthCurveDragSample_,
        depthCurveDragValue_);
      ImGui::TextWrapped("Drag across the graph to remap brightness from near to far camera distance.");
    }
    ImGui::TextWrapped("Controls: left click a point to add a red sphere, drag its red square grip to resize, right click a sphere to delete it, backspace marks and confirms deletion, right drag orbit, middle drag or Shift+right drag pan, wheel zoom.");

    if (hasHideBoxes) {
      ImGui::Separator();
      ImGui::TextUnformatted("Hide boxes");
      if (ImGui::BeginListBox("##hide-boxes", ImVec2(-FLT_MIN, 110.0f))) {
        for (std::size_t index = 0; index < hideBoxes_.size(); ++index) {
          const bool selected = static_cast<int>(index) == selectedHideBox_;
          std::string label = "Hide box " + std::to_string(index + 1);
          if (ImGui::Selectable(label.c_str(), selected)) {
            selectedHideBox_ = static_cast<int>(index);
            ResetHideBoxGizmo();
          }
          if (selected) {
            ImGui::SetItemDefaultFocus();
          }
        }
        ImGui::EndListBox();
      }

      if (selectedHideBox_ >= 0 && selectedHideBox_ < static_cast<int>(hideBoxes_.size())) {
        HideBox& hideBox = hideBoxes_[static_cast<std::size_t>(selectedHideBox_)];

        ImGui::Separator();
        ImGui::Text("Viewport gizmo: Hide box %d", selectedHideBox_ + 1);
        int gizmoMode = static_cast<int>(hideBoxGizmoMode_);
        if (ImGui::RadioButton("Move", gizmoMode == static_cast<int>(HideBoxGizmoMode::kMove))) {
          hideBoxGizmoMode_ = HideBoxGizmoMode::kMove;
        }
        ImGui::SameLine();
        if (ImGui::RadioButton("Scale", gizmoMode == static_cast<int>(HideBoxGizmoMode::kScale))) {
          hideBoxGizmoMode_ = HideBoxGizmoMode::kScale;
        }
        ImGui::SameLine();
        if (ImGui::RadioButton("Rotate", gizmoMode == static_cast<int>(HideBoxGizmoMode::kRotate))) {
          hideBoxGizmoMode_ = HideBoxGizmoMode::kRotate;
        }
        ImGui::Text("Center: %.3f, %.3f, %.3f", hideBox.center.x, hideBox.center.y, hideBox.center.z);
        ImGui::Text("Size: %.3f, %.3f, %.3f", hideBox.halfSize.x * 2.0f, hideBox.halfSize.y * 2.0f, hideBox.halfSize.z * 2.0f);
        ImGui::Text(
          "Rotation: %.1f, %.1f, %.1f deg",
          hideBox.rotationDegrees.x,
          hideBox.rotationDegrees.y,
          hideBox.rotationDegrees.z);
        ImGui::TextWrapped("Drag the highlighted gizmo handles on the box. Visible points update when you release the mouse.");
      }
    }
  }
  ImGui::End();

  if (pointContextMenuOpenRequested_) {
    ImGui::OpenPopup("PointSphereContextMenu");
    pointContextMenuOpenRequested_ = false;
  }
  ImGui::SetNextWindowPos({pointContextMenuMouseX_, pointContextMenuMouseY_}, ImGuiCond_Appearing);
  if (ImGui::BeginPopup("PointSphereContextMenu")) {
    const bool validSelection =
      pointContextMenuSelection_ >= 0 &&
      pointContextMenuSelection_ < static_cast<int>(pointSelections_.size());
    if (!validSelection) {
      pointContextMenuSelection_ = -1;
      ImGui::CloseCurrentPopup();
    } else if (ImGui::MenuItem("Delete point sphere")) {
      DeletePointSelection(pointContextMenuSelection_);
      pointContextMenuSelection_ = -1;
      ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
  }

  if (deletionWorkflowState_ != DeletionWorkflowState::kIdle || deletionConfirmPending_) {
    const ImVec2 modalPos = {
      viewport->WorkPos.x + viewport->WorkSize.x - 20.0f,
      viewport->WorkPos.y + viewport->WorkSize.y - 20.0f,
    };
    ImGui::SetNextWindowPos(modalPos, ImGuiCond_Always, {1.0f, 1.0f});
    ImGui::SetNextWindowBgAlpha(0.94f);
    ImGuiWindowFlags deleteFlags =
      ImGuiWindowFlags_AlwaysAutoResize |
      ImGuiWindowFlags_NoCollapse;
    if (ImGui::Begin("Delete Points", nullptr, deleteFlags)) {
      if (deletionWorkflowState_ == DeletionWorkflowState::kMarking) {
        ImGui::TextUnformatted("Marking points for deletion...");
        ImGui::Text(
          "Candidates: %llu / %llu",
          static_cast<unsigned long long>(deletionProcessCursor_),
          static_cast<unsigned long long>(deletionCandidateIndices_.size()));
      } else if (deletionWorkflowState_ == DeletionWorkflowState::kDeleting) {
        ImGui::TextUnformatted("Deleting marked points...");
        ImGui::Text(
          "Points: %llu / %llu",
          static_cast<unsigned long long>(deletionProcessCursor_),
          static_cast<unsigned long long>(currentCloud_.points.size()));
      } else if (deletionConfirmPending_) {
        ImGui::TextUnformatted("Points marked red will be deleted. Tap backspace again to confirm.");
        ImGui::Text("Marked points: %llu", static_cast<unsigned long long>(deletionMarkedCount_));
        if (ImGui::Button("Cancel")) {
          CancelDeletionWorkflow();
        }
      }
    }
    ImGui::End();
  }

  RenderSelectIsolatedDialog();
}

void Application::RenderSelectIsolatedDialog() {
  if (!selectIsolatedDialogOpen_) {
    return;
  }

  const bool deletionWorkflowBusy =
    deletionWorkflowState_ != DeletionWorkflowState::kIdle || deletionConfirmPending_;
  bool dialogOpen = selectIsolatedDialogOpen_;
  ImGui::SetNextWindowSize(ImVec2(360.0f, 0.0f), ImGuiCond_FirstUseEver);
  if (ImGui::Begin("Select isolated", &dialogOpen, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_AlwaysAutoResize)) {
    const bool isolatedWorkflowBusy = isolatedSelectionWorkflowState_ != IsolatedSelectionWorkflowState::kIdle;
    const bool controlsDisabled = currentCloud_.points.empty() || isolatedWorkflowBusy || deletionWorkflowBusy;

    if (controlsDisabled) {
      ImGui::BeginDisabled();
    }
    const float previousDistance = isolatedNeighborDistance_;
    if (ImGui::InputFloat("Minimum neighbor distance", &isolatedNeighborDistance_, 0.01f, 0.1f, "%.6g")) {
      if (isolatedPreviewValid_ && isolatedNeighborDistance_ != previousDistance) {
        ClearIsolatedSelectionPreview();
      }
    }
    if (controlsDisabled) {
      ImGui::EndDisabled();
    }

    ImGui::TextWrapped("Checks currently visible points and matches any point with no neighbor within the given distance.");

    if (isolatedSelectionWorkflowState_ == IsolatedSelectionWorkflowState::kCollecting) {
      const float progress = currentCloud_.points.empty()
        ? 1.0f
        : static_cast<float>(isolatedProcessCursor_) / static_cast<float>(currentCloud_.points.size());
      ImGui::TextUnformatted("Searching isolated points...");
      ImGui::ProgressBar((std::clamp)(progress, 0.0f, 1.0f), ImVec2(-1.0f, 0.0f));
      ImGui::Text(
        "Preparing: %llu / %llu",
        static_cast<unsigned long long>(isolatedProcessCursor_),
        static_cast<unsigned long long>(currentCloud_.points.size()));
    } else if (isolatedSelectionWorkflowState_ == IsolatedSelectionWorkflowState::kSearching) {
      const float progress = isolatedSearchCellKeys_.empty()
        ? 1.0f
        : static_cast<float>(isolatedProcessCursor_) / static_cast<float>(isolatedSearchCellKeys_.size());
      ImGui::TextUnformatted("Searching isolated points...");
      ImGui::ProgressBar((std::clamp)(progress, 0.0f, 1.0f), ImVec2(-1.0f, 0.0f));
      ImGui::Text(
        "Cells: %llu / %llu",
        static_cast<unsigned long long>(isolatedProcessCursor_),
        static_cast<unsigned long long>(isolatedSearchCellKeys_.size()));
    } else if (isolatedSelectionWorkflowState_ == IsolatedSelectionWorkflowState::kFinalizing) {
      const std::size_t total = isolatedPreviewDistance_ > 0.0f ? isolatedSearchCellKeys_.size() : currentCloud_.points.size();
      const float progress = total == 0
        ? 1.0f
        : static_cast<float>(isolatedProcessCursor_) / static_cast<float>(total);
      ImGui::TextUnformatted("Finalizing isolated points...");
      ImGui::ProgressBar((std::clamp)(progress, 0.0f, 1.0f), ImVec2(-1.0f, 0.0f));
      ImGui::Text(
        isolatedPreviewDistance_ > 0.0f ? "Cells: %llu / %llu" : "Points: %llu / %llu",
        static_cast<unsigned long long>(isolatedProcessCursor_),
        static_cast<unsigned long long>(total));
    } else if (isolatedSelectionWorkflowState_ == IsolatedSelectionWorkflowState::kDeleting) {
      const float progress = currentCloud_.points.empty()
        ? 1.0f
        : static_cast<float>(isolatedProcessCursor_) / static_cast<float>(currentCloud_.points.size());
      ImGui::TextUnformatted("Deleting isolated points...");
      ImGui::ProgressBar((std::clamp)(progress, 0.0f, 1.0f), ImVec2(-1.0f, 0.0f));
      ImGui::Text(
        "Points: %llu / %llu",
        static_cast<unsigned long long>(isolatedProcessCursor_),
        static_cast<unsigned long long>(currentCloud_.points.size()));
    } else if (isolatedPreviewValid_) {
      ImGui::Text("Matched points: %llu", static_cast<unsigned long long>(isolatedMatchedCount_));
    }

    if (deletionWorkflowBusy) {
      ImGui::TextWrapped("Finish or cancel the current sphere deletion workflow before running this search.");
    } else if (currentCloud_.points.empty()) {
      ImGui::TextUnformatted("Load a point cloud to use this tool.");
    }

    if (controlsDisabled) {
      ImGui::BeginDisabled();
    }
    if (ImGui::Button("Preview")) {
      StartIsolatedSelectionPreview();
    }
    if (controlsDisabled) {
      ImGui::EndDisabled();
    }

    ImGui::SameLine();
    const bool canDeleteIsolatedPoints =
      isolatedSelectionWorkflowState_ == IsolatedSelectionWorkflowState::kIdle &&
      isolatedPreviewValid_ &&
      isolatedMatchedCount_ > 0 &&
      !deletionWorkflowBusy;
    if (!canDeleteIsolatedPoints) {
      ImGui::BeginDisabled();
    }
    if (ImGui::Button("Delete")) {
      StartIsolatedSelectionDeletion();
    }
    if (!canDeleteIsolatedPoints) {
      ImGui::EndDisabled();
    }
  }
  ImGui::End();

  if (!dialogOpen) {
    selectIsolatedDialogOpen_ = false;
    if (isolatedSelectionWorkflowState_ != IsolatedSelectionWorkflowState::kDeleting) {
      ClearIsolatedSelectionPreview();
    }
    return;
  }

  selectIsolatedDialogOpen_ = true;
}

void Application::UpdateHideBoxGizmo() {
  hideBoxGizmoHotAxis_ = HideBoxGizmoAxis::kNone;
  hideBoxGizmoHovered_ = false;

  ImGuiIO& io = ImGui::GetIO();
  const ImGuiViewport* viewport = ImGui::GetMainViewport();
  if (viewport == nullptr) {
    ResetHideBoxGizmo();
    return;
  }

  const ImVec2 viewportOrigin = viewport->Pos;
  const ImVec2 viewportSize = viewport->Size;
  if (
    viewportSize.x <= 0.0f ||
    viewportSize.y <= 0.0f ||
    selectedHideBox_ < 0 ||
    selectedHideBox_ >= static_cast<int>(hideBoxes_.size()) ||
    !hideBoxesVisible_ ||
    !currentCloud_.bounds.IsValid()) {
    if (hideBoxGizmoDrag_.active && !ImGui::IsMouseDown(0) && hideBoxGizmoDrag_.dirty) {
      CommitHideBoxes();
    }
    ResetHideBoxGizmo();
    return;
  }

  auto axisEnumFromIndex = [](int axisIndex) {
    switch (axisIndex) {
      case 0:
        return HideBoxGizmoAxis::kX;
      case 1:
        return HideBoxGizmoAxis::kY;
      default:
        return HideBoxGizmoAxis::kZ;
    }
  };

  auto axisIndexFromEnum = [](HideBoxGizmoAxis axis) {
    switch (axis) {
      case HideBoxGizmoAxis::kX:
        return 0;
      case HideBoxGizmoAxis::kY:
        return 1;
      case HideBoxGizmoAxis::kZ:
        return 2;
      default:
        return -1;
    }
  };

  const HideBox& selectedBox = hideBoxes_[static_cast<std::size_t>(selectedHideBox_)];
  const float distanceToBox = (std::max)(Length(selectedBox.center - camera_.Position()), 0.01f);
  const float gizmoSize = (std::max)(currentCloud_.bounds.Radius() * 0.18f, distanceToBox * 0.20f);
  const float rotateRadius = gizmoSize * kGizmoRotateRadiusFactor;
  const Mat4 boxRotation = EulerRotationXYZ(selectedBox.rotationDegrees);
  Vec3 localAxes[3] = {
    Normalize(TransformVector(boxRotation, UnitAxis(0))),
    Normalize(TransformVector(boxRotation, UnitAxis(1))),
    Normalize(TransformVector(boxRotation, UnitAxis(2))),
  };
  const Mat4 viewProjection = camera_.ViewProjection(viewportSize.x / viewportSize.y);
  ImDrawList* drawList = ImGui::GetBackgroundDrawList();
  const ImVec2 mousePosition = io.MousePos;
  const bool mouseInViewport =
    mousePosition.x >= viewportOrigin.x &&
    mousePosition.y >= viewportOrigin.y &&
    mousePosition.x <= viewportOrigin.x + viewportSize.x &&
    mousePosition.y <= viewportOrigin.y + viewportSize.y;

  int hotAxisIndex = -1;
  float bestDistanceSquared = kGizmoHitRadiusPixels * kGizmoHitRadiusPixels;

  if (!hideBoxGizmoDrag_.active && mouseInViewport && !io.WantCaptureMouse) {
    if (
      hideBoxGizmoMode_ == HideBoxGizmoMode::kMove ||
      hideBoxGizmoMode_ == HideBoxGizmoMode::kScale) {
      ImVec2 centerScreen;
      if (ProjectToScreen(selectedBox.center, viewProjection, viewportOrigin, viewportSize, centerScreen)) {
        for (int axisIndex = 0; axisIndex < 3; ++axisIndex) {
          ImVec2 endScreen;
          const Vec3 endPoint = selectedBox.center + localAxes[axisIndex] * gizmoSize;
          if (!ProjectToScreen(endPoint, viewProjection, viewportOrigin, viewportSize, endScreen)) {
            continue;
          }

          float distanceSquared = DistanceToSegmentSquared(mousePosition, centerScreen, endScreen);
          if (hideBoxGizmoMode_ == HideBoxGizmoMode::kScale) {
            distanceSquared = (std::min)(distanceSquared, LengthSquared(Subtract(mousePosition, endScreen)));
          }
          if (distanceSquared < bestDistanceSquared) {
            bestDistanceSquared = distanceSquared;
            hotAxisIndex = axisIndex;
          }
        }
      }
    } else if (hideBoxGizmoMode_ == HideBoxGizmoMode::kRotate) {
      for (int axisIndex = 0; axisIndex < 3; ++axisIndex) {
        Vec3 tangent;
        Vec3 bitangent;
        BuildPlaneBasis(localAxes[axisIndex], tangent, bitangent);

        for (int segmentIndex = 0; segmentIndex < kGizmoCircleSegments; ++segmentIndex) {
          const float angleA = static_cast<float>(segmentIndex) / static_cast<float>(kGizmoCircleSegments) * 6.28318530718f;
          const float angleB = static_cast<float>(segmentIndex + 1) / static_cast<float>(kGizmoCircleSegments) * 6.28318530718f;
          const Vec3 pointA =
            selectedBox.center +
            tangent * (std::cos(angleA) * rotateRadius) +
            bitangent * (std::sin(angleA) * rotateRadius);
          const Vec3 pointB =
            selectedBox.center +
            tangent * (std::cos(angleB) * rotateRadius) +
            bitangent * (std::sin(angleB) * rotateRadius);
          ImVec2 screenA;
          ImVec2 screenB;
          if (!ProjectToScreen(pointA, viewProjection, viewportOrigin, viewportSize, screenA) ||
              !ProjectToScreen(pointB, viewProjection, viewportOrigin, viewportSize, screenB)) {
            continue;
          }

          const float distanceSquared = DistanceToSegmentSquared(mousePosition, screenA, screenB);
          if (distanceSquared < bestDistanceSquared) {
            bestDistanceSquared = distanceSquared;
            hotAxisIndex = axisIndex;
          }
        }
      }
    }
  }

  if (!hideBoxGizmoDrag_.active && hotAxisIndex >= 0 && ImGui::IsMouseClicked(0) && mouseInViewport && !io.WantCaptureMouse) {
    hideBoxGizmoDrag_.active = true;
    hideBoxGizmoDrag_.dirty = false;
    hideBoxGizmoDrag_.mode = hideBoxGizmoMode_;
    hideBoxGizmoDrag_.axis = axisEnumFromIndex(hotAxisIndex);
    hideBoxGizmoDrag_.boxIndex = selectedHideBox_;
    hideBoxGizmoDrag_.startCenter = selectedBox.center;
    hideBoxGizmoDrag_.startHalfSize = selectedBox.halfSize;
    hideBoxGizmoDrag_.startRotationDegrees = selectedBox.rotationDegrees;

    const CameraRay ray = BuildCameraRay(
      camera_,
      mousePosition.x - viewportOrigin.x,
      mousePosition.y - viewportOrigin.y,
      viewportSize.x,
      viewportSize.y);
    const Vec3 axisWorld = localAxes[hotAxisIndex];

    if (hideBoxGizmoMode_ == HideBoxGizmoMode::kRotate) {
      Vec3 hitPoint;
      if (IntersectRayPlane(ray, selectedBox.center, axisWorld, hitPoint)) {
        Vec3 projected = hitPoint - selectedBox.center;
        projected = projected - axisWorld * Dot(projected, axisWorld);
        if (Length(projected) > kRotationDragPlaneEpsilon) {
          hideBoxGizmoDrag_.startPlaneVector = Normalize(projected);
        } else {
          Vec3 tangent;
          Vec3 bitangent;
          BuildPlaneBasis(axisWorld, tangent, bitangent);
          hideBoxGizmoDrag_.startPlaneVector = tangent;
        }
      } else {
        Vec3 tangent;
        Vec3 bitangent;
        BuildPlaneBasis(axisWorld, tangent, bitangent);
        hideBoxGizmoDrag_.startPlaneVector = tangent;
      }
    } else {
      const Vec3 planeNormal = BuildPlaneNormalForAxisDrag(axisWorld, camera_.Position() - selectedBox.center);
      Vec3 hitPoint;
      if (IntersectRayPlane(ray, selectedBox.center, planeNormal, hitPoint)) {
        hideBoxGizmoDrag_.startAxisParameter = Dot(hitPoint - selectedBox.center, axisWorld);
      } else if (hideBoxGizmoMode_ == HideBoxGizmoMode::kScale) {
        hideBoxGizmoDrag_.startAxisParameter = GetAxisComponent(selectedBox.halfSize, hotAxisIndex);
      } else {
        hideBoxGizmoDrag_.startAxisParameter = 0.0f;
      }
    }
  }

  if (hideBoxGizmoDrag_.active) {
    const int dragAxisIndex = axisIndexFromEnum(hideBoxGizmoDrag_.axis);
    if (dragAxisIndex >= 0 && hideBoxGizmoDrag_.boxIndex >= 0 && hideBoxGizmoDrag_.boxIndex < static_cast<int>(hideBoxes_.size())) {
      HideBox& dragBox = hideBoxes_[static_cast<std::size_t>(hideBoxGizmoDrag_.boxIndex)];
      const Mat4 startRotation = EulerRotationXYZ(hideBoxGizmoDrag_.startRotationDegrees);
      const Vec3 axisWorld = Normalize(TransformVector(startRotation, UnitAxis(dragAxisIndex)));

      if (ImGui::IsMouseDown(0)) {
        const CameraRay ray = BuildCameraRay(
          camera_,
          mousePosition.x - viewportOrigin.x,
          mousePosition.y - viewportOrigin.y,
          viewportSize.x,
          viewportSize.y);

        if (hideBoxGizmoDrag_.mode == HideBoxGizmoMode::kRotate) {
          Vec3 hitPoint;
          if (IntersectRayPlane(ray, hideBoxGizmoDrag_.startCenter, axisWorld, hitPoint)) {
            Vec3 projected = hitPoint - hideBoxGizmoDrag_.startCenter;
            projected = projected - axisWorld * Dot(projected, axisWorld);
            if (Length(projected) > kRotationDragPlaneEpsilon) {
              const Vec3 currentVector = Normalize(projected);
              const float angleDegrees = SignedAngleRadians(hideBoxGizmoDrag_.startPlaneVector, currentVector, axisWorld) * kRadiansToDegrees;
              dragBox.center = hideBoxGizmoDrag_.startCenter;
              dragBox.halfSize = hideBoxGizmoDrag_.startHalfSize;
              dragBox.rotationDegrees = hideBoxGizmoDrag_.startRotationDegrees;
              SetAxisComponent(
                dragBox.rotationDegrees,
                dragAxisIndex,
                GetAxisComponent(hideBoxGizmoDrag_.startRotationDegrees, dragAxisIndex) + angleDegrees);
              hideBoxGizmoDrag_.dirty = true;
            }
          }
        } else {
          const Vec3 planeNormal = BuildPlaneNormalForAxisDrag(axisWorld, camera_.Position() - hideBoxGizmoDrag_.startCenter);
          Vec3 hitPoint;
          if (IntersectRayPlane(ray, hideBoxGizmoDrag_.startCenter, planeNormal, hitPoint)) {
            const float axisParameter = Dot(hitPoint - hideBoxGizmoDrag_.startCenter, axisWorld);
            const float delta = axisParameter - hideBoxGizmoDrag_.startAxisParameter;
            dragBox.center = hideBoxGizmoDrag_.startCenter;
            dragBox.halfSize = hideBoxGizmoDrag_.startHalfSize;
            dragBox.rotationDegrees = hideBoxGizmoDrag_.startRotationDegrees;
            if (hideBoxGizmoDrag_.mode == HideBoxGizmoMode::kMove) {
              dragBox.center = hideBoxGizmoDrag_.startCenter + axisWorld * delta;
            } else {
              SetAxisComponent(
                dragBox.halfSize,
                dragAxisIndex,
                (std::max)(kGizmoMinHalfSize, GetAxisComponent(hideBoxGizmoDrag_.startHalfSize, dragAxisIndex) + delta));
            }
            hideBoxGizmoDrag_.dirty = true;
          }
        }
      } else {
        if (hideBoxGizmoDrag_.dirty) {
          CommitHideBoxes();
        }
        hideBoxGizmoDrag_.active = false;
        hideBoxGizmoDrag_.dirty = false;
      }

      hotAxisIndex = dragAxisIndex;
    } else {
      ResetHideBoxGizmo();
      return;
    }
  }

  hideBoxGizmoHovered_ = hideBoxGizmoDrag_.active || hotAxisIndex >= 0;
  if (hotAxisIndex >= 0) {
    hideBoxGizmoHotAxis_ = axisEnumFromIndex(hotAxisIndex);
  }

  const HideBox& displayBox = hideBoxes_[static_cast<std::size_t>(selectedHideBox_)];
  const Mat4 displayRotation = EulerRotationXYZ(displayBox.rotationDegrees);
  Vec3 displayAxes[3] = {
    Normalize(TransformVector(displayRotation, UnitAxis(0))),
    Normalize(TransformVector(displayRotation, UnitAxis(1))),
    Normalize(TransformVector(displayRotation, UnitAxis(2))),
  };

  ImVec2 centerScreen;
  if (ProjectToScreen(displayBox.center, viewProjection, viewportOrigin, viewportSize, centerScreen)) {
    drawList->AddCircleFilled(centerScreen, 5.0f, IM_COL32(230, 230, 230, 220));
  }

  if (hideBoxGizmoMode_ == HideBoxGizmoMode::kMove || hideBoxGizmoMode_ == HideBoxGizmoMode::kScale) {
    for (int axisIndex = 0; axisIndex < 3; ++axisIndex) {
      ImVec2 startScreen;
      ImVec2 endScreen;
      const Vec3 endPoint = displayBox.center + displayAxes[axisIndex] * gizmoSize;
      if (!ProjectToScreen(displayBox.center, viewProjection, viewportOrigin, viewportSize, startScreen) ||
          !ProjectToScreen(endPoint, viewProjection, viewportOrigin, viewportSize, endScreen)) {
        continue;
      }

      const bool highlighted = hotAxisIndex == axisIndex;
      const ImU32 color = AxisColor(axisIndex, highlighted);
      const float thickness = highlighted ? 3.0f : 2.0f;
      drawList->AddLine(startScreen, endScreen, color, thickness);

      if (hideBoxGizmoMode_ == HideBoxGizmoMode::kMove) {
        const ImVec2 direction = Normalize2D(Subtract(endScreen, startScreen));
        const ImVec2 perpendicular = {-direction.y, direction.x};
        const ImVec2 arrowBase = Subtract(endScreen, Multiply(direction, kGizmoArrowHeadPixels));
        drawList->AddTriangleFilled(
          endScreen,
          Add(arrowBase, Multiply(perpendicular, kGizmoArrowHeadPixels * 0.45f)),
          Add(arrowBase, Multiply(perpendicular, -kGizmoArrowHeadPixels * 0.45f)),
          color);
      } else {
        const float handleRadius = highlighted ? 6.0f : 5.0f;
        drawList->AddRectFilled(
          {endScreen.x - handleRadius, endScreen.y - handleRadius},
          {endScreen.x + handleRadius, endScreen.y + handleRadius},
          color,
          1.5f);
      }
    }
  } else {
    for (int axisIndex = 0; axisIndex < 3; ++axisIndex) {
      Vec3 tangent;
      Vec3 bitangent;
      BuildPlaneBasis(displayAxes[axisIndex], tangent, bitangent);

      const bool highlighted = hotAxisIndex == axisIndex;
      const ImU32 color = AxisColor(axisIndex, highlighted);
      const float thickness = highlighted ? 3.0f : 2.0f;

      for (int segmentIndex = 0; segmentIndex < kGizmoCircleSegments; ++segmentIndex) {
        const float angleA = static_cast<float>(segmentIndex) / static_cast<float>(kGizmoCircleSegments) * 6.28318530718f;
        const float angleB = static_cast<float>(segmentIndex + 1) / static_cast<float>(kGizmoCircleSegments) * 6.28318530718f;
        const Vec3 pointA =
          displayBox.center +
          tangent * (std::cos(angleA) * rotateRadius) +
          bitangent * (std::sin(angleA) * rotateRadius);
        const Vec3 pointB =
          displayBox.center +
          tangent * (std::cos(angleB) * rotateRadius) +
          bitangent * (std::sin(angleB) * rotateRadius);
        ImVec2 screenA;
        ImVec2 screenB;
        if (!ProjectToScreen(pointA, viewProjection, viewportOrigin, viewportSize, screenA) ||
            !ProjectToScreen(pointB, viewProjection, viewportOrigin, viewportSize, screenB)) {
          continue;
        }
        drawList->AddLine(screenA, screenB, color, thickness);
      }
    }
  }
}

void Application::UpdatePointSelectionInteraction() {
  pointScaleHandleHovered_ = false;
  pointScaleHandleHotSelection_ = -1;
  const double now = glfwGetTime();

  ImGuiIO& io = ImGui::GetIO();
  const ImGuiViewport* viewport = ImGui::GetMainViewport();
  if (viewport == nullptr) {
    hoveredPointActive_ = false;
    hoverPickCache_.valid = false;
    pointScaleDrag_ = {};
    return;
  }

  const ImVec2 viewportOrigin = viewport->Pos;
  const ImVec2 viewportSize = viewport->Size;
  const ImVec2 mousePosition = io.MousePos;
  const bool mouseInViewport =
    mousePosition.x >= viewportOrigin.x &&
    mousePosition.y >= viewportOrigin.y &&
    mousePosition.x <= viewportOrigin.x + viewportSize.x &&
    mousePosition.y <= viewportOrigin.y + viewportSize.y;

  if (
    viewportSize.x <= 0.0f ||
    viewportSize.y <= 0.0f ||
    currentCloud_.points.empty()) {
    hoveredPointActive_ = false;
    hoverPickCache_.valid = false;
    pointScaleDrag_ = {};
    return;
  }

  auto pointPositionAt = [&](std::size_t pointIndex) {
    return PointPosition(currentCloud_.points[pointIndex]);
  };

  auto hitTestPointSphere = [&](const Mat4& viewProjection, int& hitSelectionIndex) {
    hitSelectionIndex = -1;
    Vec3 forward;
    Vec3 right;
    Vec3 up;
    BuildCameraBasis(camera_, forward, right, up);
    const Vec3 eye = camera_.Position();
    float bestDistanceSquared = FLT_MAX;
    float bestCameraDistance = FLT_MAX;

    for (std::size_t selectionIndex = 0; selectionIndex < pointSelections_.size(); ++selectionIndex) {
      const PointSelection& selection = pointSelections_[selectionIndex];
      if (selection.pointIndex >= currentCloud_.points.size()) {
        continue;
      }

      const Vec3 center = pointPositionAt(selection.pointIndex);
      if (IsPointHidden(center, committedHideBoxes_)) {
        continue;
      }

      ImVec2 centerScreen;
      ImVec2 radiusScreenA;
      ImVec2 radiusScreenB;
      if (
        !ProjectToScreen(center, viewProjection, viewportOrigin, viewportSize, centerScreen) ||
        !ProjectToScreen(center + right * selection.radius, viewProjection, viewportOrigin, viewportSize, radiusScreenA) ||
        !ProjectToScreen(center + up * selection.radius, viewProjection, viewportOrigin, viewportSize, radiusScreenB)) {
        continue;
      }

      const float screenRadius = (std::max)(
        std::sqrt(DistanceSquared(centerScreen, radiusScreenA)),
        std::sqrt(DistanceSquared(centerScreen, radiusScreenB)));
      const float hitRadius = screenRadius + kPointContextHitPaddingPixels;
      const float distanceSquared = DistanceSquared(mousePosition, centerScreen);
      if (distanceSquared > hitRadius * hitRadius) {
        continue;
      }

      const float cameraDistance = Length(center - eye);
      if (
        hitSelectionIndex < 0 ||
        distanceSquared < bestDistanceSquared - 0.25f ||
        (std::abs(distanceSquared - bestDistanceSquared) <= 0.25f && cameraDistance < bestCameraDistance)) {
        hitSelectionIndex = static_cast<int>(selectionIndex);
        bestDistanceSquared = distanceSquared;
        bestCameraDistance = cameraDistance;
      }
    }

    return hitSelectionIndex >= 0;
  };

  auto drawScaleHandles = [&]() {
    const Mat4 viewProjection = camera_.ViewProjection(viewportSize.x / viewportSize.y);
    const Vec3 handleDirection = BuildScaleHandleDirection(camera_);
    ImDrawList* drawList = ImGui::GetBackgroundDrawList();
    for (std::size_t selectionIndex = 0; selectionIndex < pointSelections_.size(); ++selectionIndex) {
      const PointSelection& selection = pointSelections_[selectionIndex];
      if (selection.pointIndex >= currentCloud_.points.size()) {
        continue;
      }

      const Vec3 center = pointPositionAt(selection.pointIndex);
      if (IsPointHidden(center, committedHideBoxes_)) {
        continue;
      }

      ImVec2 centerScreen;
      ImVec2 handleScreen;
      if (
        !ProjectToScreen(center, viewProjection, viewportOrigin, viewportSize, centerScreen) ||
        !ProjectToScreen(center + handleDirection * selection.radius, viewProjection, viewportOrigin, viewportSize, handleScreen)) {
        continue;
      }

      const bool active = static_cast<int>(selectionIndex) == activePointSelection_;
      const bool hot = static_cast<int>(selectionIndex) == pointScaleHandleHotSelection_ || (pointScaleDrag_.active && pointScaleDrag_.selectionIndex == static_cast<int>(selectionIndex));
      const ImU32 lineColor = hot || active ? IM_COL32(255, 96, 96, 255) : IM_COL32(214, 48, 49, 220);
      const ImU32 handleColor = hot || active ? IM_COL32(255, 128, 128, 255) : IM_COL32(224, 64, 64, 235);
      const float thickness = hot || active ? 2.5f : 1.8f;
      const float handleRadius = hot || active ? 6.0f : 5.0f;
      drawList->AddLine(centerScreen, handleScreen, lineColor, thickness);
      drawList->AddRectFilled(
        {handleScreen.x - handleRadius, handleScreen.y - handleRadius},
        {handleScreen.x + handleRadius, handleScreen.y + handleRadius},
        handleColor,
        1.5f);
    }
  };

  if (deletionConfirmPending_) {
    hoveredPointActive_ = false;
    hoverPickCache_.valid = false;
    drawScaleHandles();
    return;
  }

  if (pointScaleDrag_.active) {
    if (
      pointScaleDrag_.selectionIndex < 0 ||
      pointScaleDrag_.selectionIndex >= static_cast<int>(pointSelections_.size())) {
      pointScaleDrag_ = {};
      return;
    }

    pointScaleHandleHovered_ = true;
    pointScaleHandleHotSelection_ = pointScaleDrag_.selectionIndex;
    activePointSelection_ = pointScaleDrag_.selectionIndex;

    if (!ImGui::IsMouseDown(0)) {
      pointScaleDrag_ = {};
      return;
    }

    PointSelection& selection = pointSelections_[static_cast<std::size_t>(pointScaleDrag_.selectionIndex)];
    const Vec3 center = pointPositionAt(selection.pointIndex);
    const CameraRay ray = BuildCameraRay(
      camera_,
      mousePosition.x - viewportOrigin.x,
      mousePosition.y - viewportOrigin.y,
      viewportSize.x,
      viewportSize.y);
    Vec3 hitPoint;
    if (IntersectRayPlane(ray, center, pointScaleDrag_.planeNormal, hitPoint)) {
      selection.radius = (std::max)(kPointSelectionMinRadius, Length(hitPoint - center));
    }
    drawScaleHandles();
    return;
  }

  if (!mouseInViewport || io.WantCaptureMouse || hideBoxGizmoDrag_.active || hideBoxGizmoHovered_) {
    hoveredPointActive_ = false;
    hoverPickCache_.valid = false;
    drawScaleHandles();
    return;
  }

  if (ImGui::IsMouseDown(1) || ImGui::IsMouseDown(2)) {
    hoveredPointActive_ = false;
    hoverPickCache_.valid = false;
    drawScaleHandles();
    return;
  }

  const Mat4 viewProjection = camera_.ViewProjection(viewportSize.x / viewportSize.y);
  const Vec3 handleDirection = BuildScaleHandleDirection(camera_);
  float bestHandleDistanceSquared = kPointScaleHandleRadiusPixels * kPointScaleHandleRadiusPixels;

  for (std::size_t selectionIndex = 0; selectionIndex < pointSelections_.size(); ++selectionIndex) {
    const PointSelection& selection = pointSelections_[selectionIndex];
    if (selection.pointIndex >= currentCloud_.points.size()) {
      continue;
    }

    const Vec3 center = pointPositionAt(selection.pointIndex);
    if (IsPointHidden(center, committedHideBoxes_)) {
      continue;
    }

    ImVec2 handleScreen;
    if (!ProjectToScreen(center + handleDirection * selection.radius, viewProjection, viewportOrigin, viewportSize, handleScreen)) {
      continue;
    }

    const float distanceSquared = DistanceSquared(mousePosition, handleScreen);
    if (distanceSquared < bestHandleDistanceSquared) {
      bestHandleDistanceSquared = distanceSquared;
      pointScaleHandleHovered_ = true;
      pointScaleHandleHotSelection_ = static_cast<int>(selectionIndex);
    }
  }

  if (ImGui::IsMouseClicked(1)) {
    int contextSelectionIndex = -1;
    if (hitTestPointSphere(viewProjection, contextSelectionIndex)) {
      pointContextMenuSelection_ = contextSelectionIndex;
      activePointSelection_ = contextSelectionIndex;
      pointContextMenuMouseX_ = mousePosition.x;
      pointContextMenuMouseY_ = mousePosition.y;
      pointContextMenuOpenRequested_ = true;
      consumeRightMouseOrbit_ = true;
      hoveredPointActive_ = false;
      hoverPickCache_.valid = false;
      drawScaleHandles();
      return;
    }
  }

  if (pointScaleHandleHovered_ && ImGui::IsMouseClicked(0)) {
    pointScaleDrag_.active = true;
    pointScaleDrag_.selectionIndex = pointScaleHandleHotSelection_;
    activePointSelection_ = pointScaleHandleHotSelection_;

    const PointSelection& selection = pointSelections_[static_cast<std::size_t>(pointScaleHandleHotSelection_)];
    const Vec3 center = pointPositionAt(selection.pointIndex);
    Vec3 planeNormal = Normalize(camera_.Position() - center);
    if (Length(planeNormal) <= 0.0f) {
      planeNormal = Normalize(camera_.Target() - camera_.Position());
    }
    if (Length(planeNormal) <= 0.0f) {
      planeNormal = {0.0f, 0.0f, 1.0f};
    }
    pointScaleDrag_.planeNormal = planeNormal;
    drawScaleHandles();
    return;
  }

  const Vec3 cameraPosition = camera_.Position();
  const Vec3 cameraTarget = camera_.Target();
  const float hoverDeltaX = mousePosition.x - hoverPickCache_.mouseX;
  const float hoverDeltaY = mousePosition.y - hoverPickCache_.mouseY;
  const float hoverMouseDistanceSquared = hoverDeltaX * hoverDeltaX + hoverDeltaY * hoverDeltaY;
  const bool hoverMouseChanged =
    !hoverPickCache_.valid ||
    hoverMouseDistanceSquared >= kHoverRequeryDistancePixels * kHoverRequeryDistancePixels;
  const bool hoverCameraChanged =
    !hoverPickCache_.valid ||
    !NearlyEqual(hoverPickCache_.cameraPosition, cameraPosition) ||
    !NearlyEqual(hoverPickCache_.cameraTarget, cameraTarget);
  if (hoverMouseChanged || hoverCameraChanged) {
    const PointPickResult hoverPick = PickPoint(
      currentCloud_.points,
      camera_,
      viewProjection,
      viewportOrigin,
      viewportSize,
      mousePosition,
      ComputePickRadiusPixels(pointSize_, kPointHoverPaddingPixels),
      committedHideBoxes_,
      HoverPickStep(currentCloud_.points.size()));
    hoveredPointActive_ = hoverPick.hit;
    if (hoverPick.hit) {
      hoveredPointIndex_ = hoverPick.pointIndex;
    }
    hoverPickCache_.valid = true;
    hoverPickCache_.mouseX = mousePosition.x;
    hoverPickCache_.mouseY = mousePosition.y;
    hoverPickCache_.cameraPosition = cameraPosition;
    hoverPickCache_.cameraTarget = cameraTarget;
    hoverPickCache_.settleStartSeconds = now;
    hoverPickCache_.exactResolved = false;
  } else if (!hoverPickCache_.exactResolved && now - hoverPickCache_.settleStartSeconds >= kHoverExactSettleDelaySeconds) {
    const PointPickResult hoverPick = PickPoint(
      currentCloud_.points,
      camera_,
      viewProjection,
      viewportOrigin,
      viewportSize,
      mousePosition,
      ComputePickRadiusPixels(pointSize_, kPointHoverPaddingPixels),
      committedHideBoxes_,
      1);
    hoveredPointActive_ = hoverPick.hit;
    if (hoverPick.hit) {
      hoveredPointIndex_ = hoverPick.pointIndex;
    }
    hoverPickCache_.exactResolved = true;
  }

  if (!ImGui::IsMouseClicked(0)) {
    drawScaleHandles();
    return;
  }

  const PointPickResult clickPick = PickPoint(
    currentCloud_.points,
    camera_,
    viewProjection,
    viewportOrigin,
    viewportSize,
    mousePosition,
    ComputePickRadiusPixels(pointSize_, kPointClickPaddingPixels),
    committedHideBoxes_,
    1);
  if (!clickPick.hit) {
    drawScaleHandles();
    return;
  }

  hoveredPointActive_ = true;
  hoveredPointIndex_ = clickPick.pointIndex;
  hoverPickCache_.valid = false;

  for (std::size_t selectionIndex = 0; selectionIndex < pointSelections_.size(); ++selectionIndex) {
    if (pointSelections_[selectionIndex].pointIndex == clickPick.pointIndex) {
      activePointSelection_ = static_cast<int>(selectionIndex);
      drawScaleHandles();
      return;
    }
  }

  pointSelections_.push_back(PointSelection{
    .pointIndex = clickPick.pointIndex,
    .radius = pointSelections_.empty() ? kPointSelectionDefaultRadius : pointSelections_.back().radius,
  });
  activePointSelection_ = static_cast<int>(pointSelections_.size()) - 1;
  drawScaleHandles();
}

void Application::RenderScene() {
  int framebufferWidth = 0;
  int framebufferHeight = 0;
  glfwGetFramebufferSize(window_, &framebufferWidth, &framebufferHeight);

  glClearColor(0.05f, 0.07f, 0.10f, 1.0f);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  const std::vector<SelectionSphere> selectionSpheres = BuildSelectionSpheres();

  bool drawHoveredPoint = hoveredPointActive_ && hoveredPointIndex_ < currentCloud_.points.size();
  if (drawHoveredPoint) {
    for (const PointSelection& selection : pointSelections_) {
      if (selection.pointIndex == hoveredPointIndex_) {
        drawHoveredPoint = false;
        break;
      }
    }
    if (drawHoveredPoint) {
      const Vec3 hoveredPoint = PointPosition(currentCloud_.points[hoveredPointIndex_]);
      drawHoveredPoint = !IsPointHidden(hoveredPoint, committedHideBoxes_);
    }
  }
  const Vec3 hoveredPoint = drawHoveredPoint
    ? PointPosition(currentCloud_.points[hoveredPointIndex_])
    : Vec3{};

  renderer_.Render(
    camera_,
    framebufferWidth,
    framebufferHeight,
    pointSize_,
    pointColorMode_,
    depthColorCurve_,
    activeRenderDetail_,
    interactionPointFraction_,
    hideBoxes_,
    hideBoxesVisible_,
    selectedHideBox_,
    selectionSpheres,
    drawHoveredPoint,
    hoveredPoint);
}

std::vector<SelectionSphere> Application::BuildSelectionSpheres() const {
  std::vector<SelectionSphere> selectionSpheres;
  selectionSpheres.reserve(pointSelections_.size());
  for (const PointSelection& selection : pointSelections_) {
    if (selection.pointIndex >= currentCloud_.points.size()) {
      continue;
    }

    const Vec3 center = PointPosition(currentCloud_.points[selection.pointIndex]);
    if (IsPointHidden(center, committedHideBoxes_)) {
      continue;
    }

    selectionSpheres.push_back(SelectionSphere{
      .center = center,
      .radius = selection.radius,
    });
  }
  return selectionSpheres;
}

void Application::RebuildPointCloudRenderer() {
  currentCloud_.bounds = ComputeBounds(currentCloud_.points);
  currentCloud_.renderPointCount = static_cast<std::uint64_t>(std::count_if(
    currentCloud_.points.begin(),
    currentCloud_.points.end(),
    [](const PointVertex& point) {
      return (point.flags & kPointFlagMarkedForDeletion) == 0;
    }));
  renderer_.SetPointCloud(currentCloud_.points, currentCloud_.bounds);
  if (committedHideBoxes_.empty()) {
    visiblePointCount_ = currentCloud_.renderPointCount;
    visiblePointCountAccurate_ = true;
  } else {
    visiblePointCountAccurate_ = false;
  }
}

void Application::InvalidateDeletionSpatialIndex() {
  deletionGridValid_ = false;
  deletionGrid_.clear();
  deletionCandidateStamp_.clear();
  deletionCandidateStampValue_ = 1;
}

void Application::EnsureDeletionSpatialIndex() {
  if (deletionGridValid_) {
    return;
  }

  deletionGrid_.clear();
  deletionGridCellSize_ = currentCloud_.bounds.IsValid()
    ? (std::max)(currentCloud_.bounds.Radius() * 0.02f, 0.02f)
    : 0.02f;
  deletionCandidateStamp_.assign(currentCloud_.points.size(), 0);

  auto cellKeyForPoint = [&](const Vec3& point) {
    return DeletionGridKey{
      .x = static_cast<int>(std::floor(point.x / deletionGridCellSize_)),
      .y = static_cast<int>(std::floor(point.y / deletionGridCellSize_)),
      .z = static_cast<int>(std::floor(point.z / deletionGridCellSize_)),
    };
  };

  for (std::size_t pointIndex = 0; pointIndex < currentCloud_.points.size(); ++pointIndex) {
    deletionGrid_[cellKeyForPoint(PointPosition(currentCloud_.points[pointIndex]))].push_back(pointIndex);
  }

  deletionGridValid_ = true;
}

std::vector<std::size_t> Application::CollectDeletionCandidateIndices(const std::vector<SelectionSphere>& selectionSpheres) {
  EnsureDeletionSpatialIndex();
  if (deletionCandidateStamp_.size() != currentCloud_.points.size()) {
    deletionCandidateStamp_.assign(currentCloud_.points.size(), 0);
  }
  ++deletionCandidateStampValue_;
  if (deletionCandidateStampValue_ == 0) {
    std::fill(deletionCandidateStamp_.begin(), deletionCandidateStamp_.end(), 0);
    deletionCandidateStampValue_ = 1;
  }

  auto appendCandidatesInAabb = [&](const Bounds& bounds, std::vector<std::size_t>& candidates) {
    const DeletionGridKey minKey{
      .x = static_cast<int>(std::floor(bounds.min.x / deletionGridCellSize_)),
      .y = static_cast<int>(std::floor(bounds.min.y / deletionGridCellSize_)),
      .z = static_cast<int>(std::floor(bounds.min.z / deletionGridCellSize_)),
    };
    const DeletionGridKey maxKey{
      .x = static_cast<int>(std::floor(bounds.max.x / deletionGridCellSize_)),
      .y = static_cast<int>(std::floor(bounds.max.y / deletionGridCellSize_)),
      .z = static_cast<int>(std::floor(bounds.max.z / deletionGridCellSize_)),
    };
    for (int x = minKey.x; x <= maxKey.x; ++x) {
      for (int y = minKey.y; y <= maxKey.y; ++y) {
        for (int z = minKey.z; z <= maxKey.z; ++z) {
          const auto it = deletionGrid_.find(DeletionGridKey{x, y, z});
          if (it == deletionGrid_.end()) {
            continue;
          }
          for (std::size_t pointIndex : it->second) {
            if (deletionCandidateStamp_[pointIndex] == deletionCandidateStampValue_) {
              continue;
            }
            deletionCandidateStamp_[pointIndex] = deletionCandidateStampValue_;
            candidates.push_back(pointIndex);
          }
        }
      }
    }
  };

  std::vector<std::size_t> candidates;
  for (std::size_t sphereIndex = 0; sphereIndex < selectionSpheres.size(); ++sphereIndex) {
    const SelectionSphere& sphere = selectionSpheres[sphereIndex];
    Bounds sphereBounds;
    sphereBounds.Expand({sphere.center.x - sphere.radius, sphere.center.y - sphere.radius, sphere.center.z - sphere.radius});
    sphereBounds.Expand({sphere.center.x + sphere.radius, sphere.center.y + sphere.radius, sphere.center.z + sphere.radius});
    appendCandidatesInAabb(sphereBounds, candidates);

    if (sphereIndex == 0) {
      continue;
    }

    const SelectionSphere& previousSphere = selectionSpheres[sphereIndex - 1];
    Bounds connectorBounds;
    connectorBounds.Expand({
      previousSphere.center.x - previousSphere.radius,
      previousSphere.center.y - previousSphere.radius,
      previousSphere.center.z - previousSphere.radius,
    });
    connectorBounds.Expand({
      previousSphere.center.x + previousSphere.radius,
      previousSphere.center.y + previousSphere.radius,
      previousSphere.center.z + previousSphere.radius,
    });
    connectorBounds.Expand({
      sphere.center.x - sphere.radius,
      sphere.center.y - sphere.radius,
      sphere.center.z - sphere.radius,
    });
    connectorBounds.Expand({
      sphere.center.x + sphere.radius,
      sphere.center.y + sphere.radius,
      sphere.center.z + sphere.radius,
    });
    appendCandidatesInAabb(connectorBounds, candidates);
  }

  return candidates;
}

void Application::BeginDeletionMarking() {
  if (isolatedSelectionWorkflowState_ != IsolatedSelectionWorkflowState::kDeleting) {
    ClearIsolatedSelectionPreview();
  }
  const std::vector<SelectionSphere> selectionSpheres = BuildSelectionSpheres();
  if (selectionSpheres.empty()) {
    return;
  }

  deletionSelectionSpheres_ = selectionSpheres;
  deletionCandidateIndices_ = CollectDeletionCandidateIndices(selectionSpheres);
  deletionMarkedPointIndices_.clear();
  deletionMarkedPointIndices_.reserve(deletionCandidateIndices_.size());
  deletionMarkedCount_ = 0;
  deletionConfirmPending_ = false;
  deletionWorkflowState_ = DeletionWorkflowState::kMarking;
  deletionProcessCursor_ = 0;
  ClearPointSelections();
  hoveredPointActive_ = false;
  hoverPickCache_.valid = false;
  statusMessage_ = "Marking points for deletion...";
}

void Application::CancelDeletionWorkflow() {
  if (!deletionConfirmPending_) {
    return;
  }

  for (std::size_t pointIndex : deletionMarkedPointIndices_) {
    if (pointIndex >= currentCloud_.points.size()) {
      continue;
    }
    currentCloud_.points[pointIndex].flags &= static_cast<std::uint8_t>(~kPointFlagMarkedForDeletion);
  }
  deletionMarkedPointIndices_.clear();
  deletionCandidateIndices_.clear();
  deletionSelectionSpheres_.clear();
  deletionConfirmPending_ = false;
  deletionMarkedCount_ = 0;
  deletionWorkflowState_ = DeletionWorkflowState::kIdle;
  deletionProcessCursor_ = 0;
  renderer_.UpdatePointCloud(currentCloud_.points, currentCloud_.bounds);
  if (committedHideBoxes_.empty()) {
    visiblePointCount_ = currentCloud_.renderPointCount;
    visiblePointCountAccurate_ = true;
  } else {
    visiblePointCountAccurate_ = false;
  }
  statusMessage_ = "Deletion cancelled.";
}

void Application::ConfirmDeletion() {
  if (!deletionConfirmPending_) {
    return;
  }

  deletionConfirmPending_ = false;
  deletionWorkflowState_ = DeletionWorkflowState::kDeleting;
  deletionProcessCursor_ = 0;
  deletionWorkingPoints_.clear();
  deletionWorkingPoints_.reserve(currentCloud_.points.size() - deletionMarkedPointIndices_.size());
  ClearPointSelections();
  hoveredPointActive_ = false;
  hoverPickCache_.valid = false;
  pointContextMenuSelection_ = -1;
  pointContextMenuOpenRequested_ = false;
  statusMessage_ = "Deleting marked points...";
}

void Application::StartIsolatedSelectionPreview() {
  if (currentCloud_.points.empty()) {
    return;
  }
  if (isolatedSelectionWorkflowState_ != IsolatedSelectionWorkflowState::kIdle) {
    return;
  }
  if (deletionWorkflowState_ != DeletionWorkflowState::kIdle || deletionConfirmPending_) {
    statusMessage_ = "Finish the current sphere deletion workflow first.";
    return;
  }
  if (!std::isfinite(isolatedNeighborDistance_)) {
    statusMessage_ = "Minimum neighbor distance must be finite.";
    return;
  }

  ClearIsolatedSelectionPreview();
  isolatedPreviewDistance_ = isolatedNeighborDistance_;
  isolatedMatchedCount_ = 0;
  isolatedMatchedPointIndices_.clear();
  isolatedSearchCellKeys_.clear();
  isolatedSearchGrid_.clear();
  isolatedExactPointCounts_.clear();
  if (isolatedPreviewDistance_ > 0.0f) {
    isolatedPointHasNeighbor_.assign(currentCloud_.points.size(), 0);
  } else {
    isolatedPointHasNeighbor_.clear();
  }
  isolatedDeletionWorkingPoints_.clear();
  isolatedVisiblePointCount_ = 0;
  isolatedProcessCursor_ = 0;
  isolatedSelectionWorkflowState_ = IsolatedSelectionWorkflowState::kCollecting;
  statusMessage_ = "Searching for isolated points...";
}

void Application::StartIsolatedSelectionDeletion() {
  if (isolatedSelectionWorkflowState_ != IsolatedSelectionWorkflowState::kIdle) {
    return;
  }
  if (!isolatedPreviewValid_ || isolatedMatchedPointIndices_.empty()) {
    return;
  }

  isolatedSelectionWorkflowState_ = IsolatedSelectionWorkflowState::kDeleting;
  isolatedProcessCursor_ = 0;
  isolatedDeletionWorkingPoints_.clear();
  isolatedDeletionWorkingPoints_.reserve(currentCloud_.points.size() - isolatedMatchedPointIndices_.size());
  ClearPointSelections();
  hoveredPointActive_ = false;
  hoverPickCache_.valid = false;
  pointContextMenuSelection_ = -1;
  pointContextMenuOpenRequested_ = false;
  statusMessage_ = "Deleting isolated points...";
}

void Application::UpdateIsolatedSelectionWorkflow() {
  if (isolatedSelectionWorkflowState_ == IsolatedSelectionWorkflowState::kIdle) {
    return;
  }

  if (isolatedSelectionWorkflowState_ == IsolatedSelectionWorkflowState::kCollecting) {
    const bool hasCommittedHideBoxes = !committedHideBoxes_.empty();
    const std::size_t end = (std::min)(currentCloud_.points.size(), isolatedProcessCursor_ + kIsolatedWorkChunkPoints);
    for (std::size_t pointIndex = isolatedProcessCursor_; pointIndex < end; ++pointIndex) {
      const Vec3 position = PointPosition(currentCloud_.points[pointIndex]);
      if (hasCommittedHideBoxes && IsPointHidden(position, committedHideBoxes_)) {
        continue;
      }

      ++isolatedVisiblePointCount_;
      if (isolatedPreviewDistance_ < 0.0f) {
        continue;
      }
      if (isolatedPreviewDistance_ == 0.0f) {
        const ExactPointKey key{
          .x = std::bit_cast<std::uint32_t>(position.x),
          .y = std::bit_cast<std::uint32_t>(position.y),
          .z = std::bit_cast<std::uint32_t>(position.z),
        };
        ++isolatedExactPointCounts_[key];
        continue;
      }

      const float cellSize = isolatedPreviewDistance_;
      const DeletionGridKey cellKey{
        .x = static_cast<int>(std::floor(position.x / cellSize)),
        .y = static_cast<int>(std::floor(position.y / cellSize)),
        .z = static_cast<int>(std::floor(position.z / cellSize)),
      };
      auto [cellIt, inserted] = isolatedSearchGrid_.try_emplace(cellKey);
      if (inserted) {
        isolatedSearchCellKeys_.push_back(cellKey);
      }
      cellIt->second.push_back(pointIndex);
    }

    isolatedProcessCursor_ = end;
    if (isolatedProcessCursor_ < currentCloud_.points.size()) {
      return;
    }

    isolatedProcessCursor_ = 0;
    if (isolatedVisiblePointCount_ == 0) {
      renderer_.UpdatePointCloud(currentCloud_.points, currentCloud_.bounds);
      isolatedPreviewValid_ = true;
      isolatedMatchedCount_ = 0;
      isolatedSelectionWorkflowState_ = IsolatedSelectionWorkflowState::kIdle;
      statusMessage_ = "No visible points matched the isolated-point search.";
      isolatedSearchCellKeys_.clear();
      isolatedSearchGrid_.clear();
      isolatedExactPointCounts_.clear();
      isolatedPointHasNeighbor_.clear();
      isolatedVisiblePointCount_ = 0;
      return;
    }

    isolatedSelectionWorkflowState_ = isolatedPreviewDistance_ > 0.0f
      ? IsolatedSelectionWorkflowState::kSearching
      : IsolatedSelectionWorkflowState::kFinalizing;
    return;
  }

  if (isolatedSelectionWorkflowState_ == IsolatedSelectionWorkflowState::kSearching) {
    static constexpr DeletionGridKey kForwardNeighborOffsets[] = {
      {0, 0, 1},
      {0, 1, -1},
      {0, 1, 0},
      {0, 1, 1},
      {1, -1, -1},
      {1, -1, 0},
      {1, -1, 1},
      {1, 0, -1},
      {1, 0, 0},
      {1, 0, 1},
      {1, 1, -1},
      {1, 1, 0},
      {1, 1, 1},
    };

    const std::size_t end = (std::min)(isolatedSearchCellKeys_.size(), isolatedProcessCursor_ + kIsolatedWorkChunkCells);
    const float thresholdSquared = isolatedPreviewDistance_ * isolatedPreviewDistance_;
    for (std::size_t cellIndex = isolatedProcessCursor_; cellIndex < end; ++cellIndex) {
      const DeletionGridKey& cellKey = isolatedSearchCellKeys_[cellIndex];
      const auto cellIt = isolatedSearchGrid_.find(cellKey);
      if (cellIt == isolatedSearchGrid_.end()) {
        continue;
      }

      const std::vector<std::size_t>& pointIndices = cellIt->second;
      for (std::size_t left = 0; left < pointIndices.size(); ++left) {
        const std::size_t pointIndexA = pointIndices[left];
        const Vec3 positionA = PointPosition(currentCloud_.points[pointIndexA]);
        for (std::size_t right = left + 1; right < pointIndices.size(); ++right) {
          const std::size_t pointIndexB = pointIndices[right];
          if (isolatedPointHasNeighbor_[pointIndexA] != 0 && isolatedPointHasNeighbor_[pointIndexB] != 0) {
            continue;
          }
          if (DistanceSquared(positionA, PointPosition(currentCloud_.points[pointIndexB])) > thresholdSquared) {
            continue;
          }
          isolatedPointHasNeighbor_[pointIndexA] = 1;
          isolatedPointHasNeighbor_[pointIndexB] = 1;
        }
      }

      for (const DeletionGridKey& offset : kForwardNeighborOffsets) {
        const auto neighborIt = isolatedSearchGrid_.find(DeletionGridKey{
          .x = cellKey.x + offset.x,
          .y = cellKey.y + offset.y,
          .z = cellKey.z + offset.z,
        });
        if (neighborIt == isolatedSearchGrid_.end()) {
          continue;
        }

        const std::vector<std::size_t>& neighborPointIndices = neighborIt->second;
        for (std::size_t pointIndexA : pointIndices) {
          const Vec3 positionA = PointPosition(currentCloud_.points[pointIndexA]);
          for (std::size_t pointIndexB : neighborPointIndices) {
            if (isolatedPointHasNeighbor_[pointIndexA] != 0 && isolatedPointHasNeighbor_[pointIndexB] != 0) {
              continue;
            }
            if (DistanceSquared(positionA, PointPosition(currentCloud_.points[pointIndexB])) > thresholdSquared) {
              continue;
            }
            isolatedPointHasNeighbor_[pointIndexA] = 1;
            isolatedPointHasNeighbor_[pointIndexB] = 1;
          }
        }
      }
    }

    isolatedProcessCursor_ = end;
    if (isolatedProcessCursor_ < isolatedSearchCellKeys_.size()) {
      return;
    }

    isolatedProcessCursor_ = 0;
    isolatedSelectionWorkflowState_ = IsolatedSelectionWorkflowState::kFinalizing;
    return;
  }

  if (isolatedSelectionWorkflowState_ == IsolatedSelectionWorkflowState::kFinalizing) {
    if (isolatedPreviewDistance_ > 0.0f) {
      const std::size_t end = (std::min)(isolatedSearchCellKeys_.size(), isolatedProcessCursor_ + kIsolatedWorkChunkCells);
      for (std::size_t cellIndex = isolatedProcessCursor_; cellIndex < end; ++cellIndex) {
        const auto cellIt = isolatedSearchGrid_.find(isolatedSearchCellKeys_[cellIndex]);
        if (cellIt == isolatedSearchGrid_.end()) {
          continue;
        }
        for (std::size_t pointIndex : cellIt->second) {
          if (isolatedPointHasNeighbor_[pointIndex] != 0) {
            continue;
          }
          currentCloud_.points[pointIndex].flags |= kPointFlagIsolatedPreview;
          isolatedMatchedPointIndices_.push_back(pointIndex);
        }
      }

      isolatedProcessCursor_ = end;
      if (isolatedProcessCursor_ < isolatedSearchCellKeys_.size()) {
        return;
      }
    } else {
      const bool hasCommittedHideBoxes = !committedHideBoxes_.empty();
      const std::size_t end = (std::min)(currentCloud_.points.size(), isolatedProcessCursor_ + kIsolatedWorkChunkPoints);
      for (std::size_t pointIndex = isolatedProcessCursor_; pointIndex < end; ++pointIndex) {
        const Vec3 position = PointPosition(currentCloud_.points[pointIndex]);
        if (hasCommittedHideBoxes && IsPointHidden(position, committedHideBoxes_)) {
          continue;
        }

        bool isolated = isolatedPreviewDistance_ < 0.0f;
        if (isolatedPreviewDistance_ == 0.0f) {
          const ExactPointKey key{
            .x = std::bit_cast<std::uint32_t>(position.x),
            .y = std::bit_cast<std::uint32_t>(position.y),
            .z = std::bit_cast<std::uint32_t>(position.z),
          };
          const auto it = isolatedExactPointCounts_.find(key);
          isolated = it != isolatedExactPointCounts_.end() && it->second <= 1;
        }

        if (!isolated) {
          continue;
        }

        currentCloud_.points[pointIndex].flags |= kPointFlagIsolatedPreview;
        isolatedMatchedPointIndices_.push_back(pointIndex);
      }

      isolatedProcessCursor_ = end;
      if (isolatedProcessCursor_ < currentCloud_.points.size()) {
        return;
      }
    }

    renderer_.UpdatePointCloud(currentCloud_.points, currentCloud_.bounds);
    isolatedMatchedCount_ = isolatedMatchedPointIndices_.size();
    isolatedPreviewValid_ = true;
    isolatedSearchCellKeys_.clear();
    isolatedSearchGrid_.clear();
    isolatedExactPointCounts_.clear();
    isolatedPointHasNeighbor_.clear();
    isolatedVisiblePointCount_ = 0;
    isolatedProcessCursor_ = 0;
    isolatedSelectionWorkflowState_ = IsolatedSelectionWorkflowState::kIdle;
    if (isolatedMatchedCount_ > 0) {
      statusMessage_ = "Previewing " + std::to_string(isolatedMatchedCount_) + " isolated points.";
    } else {
      statusMessage_ = "No isolated visible points matched the search.";
    }
    return;
  }

  const std::size_t end = (std::min)(currentCloud_.points.size(), isolatedProcessCursor_ + kDeletionWorkChunkPoints);
  for (std::size_t pointIndex = isolatedProcessCursor_; pointIndex < end; ++pointIndex) {
    const PointVertex& point = currentCloud_.points[pointIndex];
    if ((point.flags & kPointFlagIsolatedPreview) != 0) {
      continue;
    }
    isolatedDeletionWorkingPoints_.push_back(point);
  }

  isolatedProcessCursor_ = end;
  if (isolatedProcessCursor_ < currentCloud_.points.size()) {
    return;
  }

  const std::size_t deletedPointCount = isolatedMatchedPointIndices_.size();
  currentCloud_.points = std::move(isolatedDeletionWorkingPoints_);
  isolatedDeletionWorkingPoints_.clear();
  isolatedMatchedPointIndices_.clear();
  isolatedSearchCellKeys_.clear();
  isolatedSearchGrid_.clear();
  isolatedExactPointCounts_.clear();
  isolatedPointHasNeighbor_.clear();
  isolatedMatchedCount_ = 0;
  isolatedPreviewValid_ = false;
  isolatedSelectionWorkflowState_ = IsolatedSelectionWorkflowState::kIdle;
  isolatedVisiblePointCount_ = 0;
  isolatedProcessCursor_ = 0;
  InvalidateDeletionSpatialIndex();
  RebuildPointCloudRenderer();
  if (!cameraTouched_ && currentCloud_.bounds.IsValid()) {
    camera_.Frame(currentCloud_.bounds);
    cameraFramed_ = true;
  }
  statusMessage_ = "Deleted " + std::to_string(deletedPointCount) + " isolated points.";
}

void Application::ClearIsolatedSelectionPreview() {
  for (std::size_t pointIndex : isolatedMatchedPointIndices_) {
    if (pointIndex >= currentCloud_.points.size()) {
      continue;
    }
    currentCloud_.points[pointIndex].flags &= static_cast<std::uint8_t>(~kPointFlagIsolatedPreview);
  }

  const bool hadPreview = isolatedPreviewValid_ || !isolatedMatchedPointIndices_.empty();
  isolatedMatchedPointIndices_.clear();
  isolatedSearchCellKeys_.clear();
  isolatedSearchGrid_.clear();
  isolatedExactPointCounts_.clear();
  isolatedPointHasNeighbor_.clear();
  isolatedDeletionWorkingPoints_.clear();
  isolatedMatchedCount_ = 0;
  isolatedPreviewValid_ = false;
  isolatedVisiblePointCount_ = 0;
  isolatedProcessCursor_ = 0;
  if (isolatedSelectionWorkflowState_ != IsolatedSelectionWorkflowState::kDeleting) {
    isolatedSelectionWorkflowState_ = IsolatedSelectionWorkflowState::kIdle;
  }

  if (hadPreview) {
    renderer_.UpdatePointCloud(currentCloud_.points, currentCloud_.bounds);
  }
}

void Application::UpdateCloudLoading() {
  for (PointCloudChunk& chunk : loader_.TakePendingChunks()) {
    currentCloud_.points.insert(currentCloud_.points.end(), chunk.points.begin(), chunk.points.end());
    InvalidateDeletionSpatialIndex();
    currentCloud_.bounds = chunk.bounds;
    currentCloud_.sourcePointCount = chunk.sourcePointCount;
    currentCloud_.renderPointCount = chunk.renderedPointCount;
    currentCloud_.sampledRender = chunk.sampledRender;
    currentCloud_.samplingStride = chunk.samplingStride;

    if (committedHideBoxes_.empty()) {
      visiblePointCount_ = currentCloud_.renderPointCount;
      visiblePointCountAccurate_ = true;
    } else {
      visiblePointCountAccurate_ = false;
    }
    renderer_.Append(chunk);

    if (!cameraFramed_ && currentCloud_.bounds.IsValid()) {
      camera_.Frame(currentCloud_.bounds);
      cameraFramed_ = true;
    }
  }

  if (std::optional<PointCloudData> loaded = loader_.TakeCompleted()) {
    InvalidateDeletionSpatialIndex();
    currentCloud_.sourcePath = loaded->sourcePath;
    currentCloud_.bounds = loaded->bounds;
    currentCloud_.sourcePointCount = loaded->sourcePointCount;
    currentCloud_.renderPointCount = loaded->renderPointCount;
    currentCloud_.sampledRender = loaded->sampledRender;
    currentCloud_.samplingStride = loaded->samplingStride;
    if (committedHideBoxes_.empty()) {
      visiblePointCount_ = currentCloud_.renderPointCount;
      visiblePointCountAccurate_ = true;
    } else {
      visiblePointCountAccurate_ = false;
    }
    if (!cameraTouched_ && currentCloud_.bounds.IsValid()) {
      camera_.Frame(currentCloud_.bounds);
    }
    statusMessage_ = "Loaded " + currentCloud_.sourcePath.filename().string();
  }
}

void Application::UpdateFrameStats() {
  const double now = glfwGetTime();
  if (lastFrameTimeSeconds_ <= 0.0) {
    lastFrameTimeSeconds_ = now;
    return;
  }

  const float frameMs = static_cast<float>((now - lastFrameTimeSeconds_) * 1000.0);
  lastFrameTimeSeconds_ = now;
  if (frameMs <= 0.0f) {
    return;
  }

  if (smoothedFrameMs_ <= 0.0f) {
    smoothedFrameMs_ = frameMs;
  } else {
    smoothedFrameMs_ = smoothedFrameMs_ * 0.9f + frameMs * 0.1f;
  }
  smoothedFps_ = smoothedFrameMs_ > 0.0f ? 1000.0f / smoothedFrameMs_ : 0.0f;

  activeRenderDetail_ = ChooseRenderDetail(interactionActive_);
  UpdateInteractionPointBudget();
}

void Application::UpdateInteractionPointBudget() {
  const double now = glfwGetTime();
  if (!interactionActive_ || renderer_.PointCount() == 0) {
    interactionTuningActive_ = false;
    interactionPointFraction_ = 1.0f;
    interactionPointFractionMin_ = 0.0f;
    interactionPointFractionMax_ = 1.0f;
    lastInteractionTuneTimeSeconds_ = now;
    return;
  }

  if (!interactionTuningActive_) {
    interactionTuningActive_ = true;
    interactionPointFraction_ = 1.0f;
    interactionPointFractionMin_ = 0.0f;
    interactionPointFractionMax_ = 1.0f;
    lastInteractionTuneTimeSeconds_ = now;
    return;
  }

  if (now - lastInteractionTuneTimeSeconds_ < kInteractionTuneIntervalSeconds) {
    return;
  }
  lastInteractionTuneTimeSeconds_ = now;

  if (smoothedFps_ < kInteractionTargetFpsMin) {
    interactionPointFractionMax_ = (std::min)(interactionPointFractionMax_, interactionPointFraction_);
    float nextFraction = interactionPointFraction_ * kInteractionPointDropFactor;
    if (interactionPointFractionMin_ > 0.0f) {
      nextFraction = (std::max)(nextFraction, (interactionPointFractionMin_ + interactionPointFraction_) * 0.5f);
    }
    interactionPointFraction_ = (std::max)(kMinInteractionPointFraction, nextFraction);
    return;
  }

  interactionPointFractionMin_ = (std::max)(interactionPointFractionMin_, interactionPointFraction_);
  if (interactionPointFraction_ >= 1.0f) {
    return;
  }

  if (smoothedFps_ >= kInteractionTargetFpsMax) {
    if (interactionPointFractionMax_ < 1.0f) {
      interactionPointFraction_ = (std::min)(1.0f, (interactionPointFraction_ + interactionPointFractionMax_) * 0.5f);
    } else {
      interactionPointFraction_ = (std::min)(1.0f, interactionPointFraction_ * 1.5f);
    }
    return;
  }

  if (smoothedFps_ >= kInteractionProbeFps && interactionPointFractionMax_ > interactionPointFraction_) {
    interactionPointFraction_ = (std::min)(1.0f, (interactionPointFraction_ + interactionPointFractionMax_) * 0.5f);
  }
}

void Application::HandleCameraInput() {
  ImGuiIO& io = ImGui::GetIO();

  double cursorX = 0.0;
  double cursorY = 0.0;
  glfwGetCursorPos(window_, &cursorX, &cursorY);

  if (firstCursorSample_) {
    lastCursorX_ = cursorX;
    lastCursorY_ = cursorY;
    firstCursorSample_ = false;
  }

  const float deltaX = static_cast<float>(cursorX - lastCursorX_);
  const float deltaY = static_cast<float>(cursorY - lastCursorY_);
  lastCursorX_ = cursorX;
  lastCursorY_ = cursorY;

  const bool commandPressed =
    glfwGetKey(window_, GLFW_KEY_LEFT_SUPER) == GLFW_PRESS ||
    glfwGetKey(window_, GLFW_KEY_RIGHT_SUPER) == GLFW_PRESS;
  const bool controlPressed =
    glfwGetKey(window_, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS ||
    glfwGetKey(window_, GLFW_KEY_RIGHT_CONTROL) == GLFW_PRESS;
  const bool shiftPressed =
    glfwGetKey(window_, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS ||
    glfwGetKey(window_, GLFW_KEY_RIGHT_SHIFT) == GLFW_PRESS;
  const bool openShortcutPressed =
#if defined(__APPLE__)
    commandPressed && glfwGetKey(window_, GLFW_KEY_O) == GLFW_PRESS;
#else
    controlPressed && glfwGetKey(window_, GLFW_KEY_O) == GLFW_PRESS;
#endif

  if (openShortcutPressed && !openShortcutLatched_ && !io.WantCaptureKeyboard) {
    StartOpenDialog();
  }
  openShortcutLatched_ = openShortcutPressed;
  interactionActive_ = hideBoxGizmoDrag_.active || pointScaleDrag_.active;

  const bool gizmoCapturingLeftMouse =
    hideBoxGizmoDrag_.active ||
    (hideBoxGizmoHovered_ && glfwGetMouseButton(window_, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS) ||
    pointScaleDrag_.active ||
    (pointScaleHandleHovered_ && glfwGetMouseButton(window_, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS);
  const bool consumeRightMouseOrbit = consumeRightMouseOrbit_;
  consumeRightMouseOrbit_ = false;

  if (!io.WantCaptureMouse && currentCloud_.bounds.IsValid()) {
    const bool middlePressed = glfwGetMouseButton(window_, GLFW_MOUSE_BUTTON_MIDDLE) == GLFW_PRESS;
    const bool rightPressed = glfwGetMouseButton(window_, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS;
    const bool panActive = middlePressed || (shiftPressed && rightPressed);

    if (rightPressed && !shiftPressed && !gizmoCapturingLeftMouse && !consumeRightMouseOrbit) {
      camera_.Rotate(deltaX, deltaY);
      cameraTouched_ = true;
      interactionActive_ = true;
    }
    if (panActive) {
      camera_.Pan(deltaX, deltaY);
      cameraTouched_ = true;
      interactionActive_ = true;
    }
    if (pendingScrollY_ != 0.0f) {
      camera_.Zoom(pendingScrollY_);
      cameraTouched_ = true;
      interactionActive_ = true;
    }
  }

  pendingScrollY_ = 0.0f;
}

void Application::HandleDeletionInput() {
  ImGuiIO& io = ImGui::GetIO();
  const bool backspacePressed = glfwGetKey(window_, GLFW_KEY_BACKSPACE) == GLFW_PRESS;
  if (!backspacePressed) {
    backspaceLatched_ = false;
    return;
  }

  if (backspaceLatched_ || io.WantTextInput) {
    return;
  }
  backspaceLatched_ = true;

  if (deletionConfirmPending_) {
    ConfirmDeletion();
    return;
  }

  if (pointSelections_.empty()) {
    return;
  }

  BeginDeletionMarking();
}

void Application::UpdateDeletionWorkflow() {
  if (deletionWorkflowState_ == DeletionWorkflowState::kIdle || deletionWorkflowState_ == DeletionWorkflowState::kConfirmPending) {
    return;
  }

  if (deletionWorkflowState_ == DeletionWorkflowState::kMarking) {
    const std::size_t end = (std::min)(deletionCandidateIndices_.size(), deletionProcessCursor_ + kDeletionWorkChunkPoints);
    for (std::size_t candidateIndex = deletionProcessCursor_; candidateIndex < end; ++candidateIndex) {
      const std::size_t pointIndex = deletionCandidateIndices_[candidateIndex];
      if (pointIndex >= currentCloud_.points.size()) {
        continue;
      }

      PointVertex& point = currentCloud_.points[pointIndex];
      if ((point.flags & kPointFlagMarkedForDeletion) != 0) {
        continue;
      }

      const Vec3 position = PointPosition(point);
      bool marked = false;
      for (std::size_t sphereIndex = 0; sphereIndex < deletionSelectionSpheres_.size(); ++sphereIndex) {
        if (IsInsideSphere(position, deletionSelectionSpheres_[sphereIndex])) {
          marked = true;
          break;
        }
        if (
          sphereIndex > 0 &&
          IsInsideConnector(position, deletionSelectionSpheres_[sphereIndex - 1], deletionSelectionSpheres_[sphereIndex])) {
          marked = true;
          break;
        }
      }
      if (!marked) {
        continue;
      }
      point.flags |= kPointFlagMarkedForDeletion;
      deletionMarkedPointIndices_.push_back(pointIndex);
    }
    deletionProcessCursor_ = end;
    if (deletionProcessCursor_ < deletionCandidateIndices_.size()) {
      return;
    }

    renderer_.UpdatePointCloud(currentCloud_.points, currentCloud_.bounds);
    if (committedHideBoxes_.empty()) {
      visiblePointCount_ = currentCloud_.renderPointCount;
      visiblePointCountAccurate_ = true;
    } else {
      visiblePointCountAccurate_ = false;
    }
    deletionMarkedCount_ = deletionMarkedPointIndices_.size();
    deletionConfirmPending_ = deletionMarkedCount_ > 0;
    deletionWorkflowState_ = deletionConfirmPending_ ? DeletionWorkflowState::kConfirmPending : DeletionWorkflowState::kIdle;
    deletionCandidateIndices_.clear();
    deletionSelectionSpheres_.clear();
    deletionProcessCursor_ = 0;
    if (deletionConfirmPending_) {
      statusMessage_ = "Marked " + std::to_string(deletionMarkedCount_) + " points for deletion. Press Backspace again to confirm.";
    } else {
      statusMessage_ = "No points fell inside the selected spheres.";
    }
    return;
  }

  const std::size_t end = (std::min)(currentCloud_.points.size(), deletionProcessCursor_ + kDeletionWorkChunkPoints);
  for (std::size_t pointIndex = deletionProcessCursor_; pointIndex < end; ++pointIndex) {
    const PointVertex& point = currentCloud_.points[pointIndex];
    if ((point.flags & kPointFlagMarkedForDeletion) != 0) {
      continue;
    }
    deletionWorkingPoints_.push_back(point);
  }

  deletionProcessCursor_ = end;
  if (deletionProcessCursor_ < currentCloud_.points.size()) {
    return;
  }

  currentCloud_.points = std::move(deletionWorkingPoints_);
  deletionWorkingPoints_.clear();
  const std::size_t deletedPointCount = deletionMarkedPointIndices_.size();
  deletionMarkedPointIndices_.clear();
  deletionMarkedCount_ = 0;
  deletionWorkflowState_ = DeletionWorkflowState::kIdle;
  deletionSelectionSpheres_.clear();
  deletionProcessCursor_ = 0;
  InvalidateDeletionSpatialIndex();
  RebuildPointCloudRenderer();
  if (!cameraTouched_ && currentCloud_.bounds.IsValid()) {
    camera_.Frame(currentCloud_.bounds);
    cameraFramed_ = true;
  }
  statusMessage_ = "Deleted " + std::to_string(deletedPointCount) + " points.";
}

void Application::StartOpenDialog() {
  if (std::optional<std::filesystem::path> selectedPath = OpenPointCloudDialog()) {
    OpenPointCloud(*selectedPath);
  }
}

void Application::ResetView() {
  if (!currentCloud_.bounds.IsValid()) {
    return;
  }

  camera_.Frame(currentCloud_.bounds);
  cameraFramed_ = true;
  cameraTouched_ = false;
  statusMessage_ = "View reset.";
}

void Application::OpenPointCloud(const std::filesystem::path& path) {
  if (path.empty()) {
    return;
  }

  if (!std::filesystem::exists(path)) {
    statusMessage_ = "Selected file does not exist.";
    return;
  }

  renderer_.Clear();
  renderer_.SetHideBoxes({});
  currentCloud_ = {};
  visiblePointCount_ = 0;
  visiblePointCountAccurate_ = true;
  hideBoxes_.clear();
  committedHideBoxes_.clear();
  hideBoxesVisible_ = true;
  selectedHideBox_ = -1;
  ResetHideBoxGizmo();
  ClearPointSelections();
  hoverPickCache_.valid = false;
  backspaceLatched_ = false;
  deletionConfirmPending_ = false;
  deletionMarkedCount_ = 0;
  deletionWorkflowState_ = DeletionWorkflowState::kIdle;
  deletionMarkedPointIndices_.clear();
  deletionCandidateIndices_.clear();
  deletionSelectionSpheres_.clear();
  deletionWorkingPoints_.clear();
  deletionProcessCursor_ = 0;
  selectIsolatedDialogOpen_ = false;
  isolatedPreviewValid_ = false;
  isolatedMatchedCount_ = 0;
  isolatedSelectionWorkflowState_ = IsolatedSelectionWorkflowState::kIdle;
  isolatedMatchedPointIndices_.clear();
  isolatedSearchCellKeys_.clear();
  isolatedSearchGrid_.clear();
  isolatedExactPointCounts_.clear();
  isolatedPointHasNeighbor_.clear();
  isolatedDeletionWorkingPoints_.clear();
  isolatedVisiblePointCount_ = 0;
  isolatedProcessCursor_ = 0;
  InvalidateDeletionSpatialIndex();
  currentCloud_.sourcePath = path;
  cameraFramed_ = false;
  cameraTouched_ = false;
  interactionActive_ = false;
  interactionTuningActive_ = false;
  interactionPointFraction_ = 1.0f;
  interactionPointFractionMin_ = 0.0f;
  interactionPointFractionMax_ = 1.0f;
  activeRenderDetail_ = RenderDetail::kFull;
  loader_.Start(path);
  statusMessage_ = "Opening " + path.filename().string();
}

void Application::AddHideBox() {
  if (!currentCloud_.bounds.IsValid()) {
    return;
  }

  hideBoxes_.push_back(MakeDefaultHideBox(currentCloud_.bounds, camera_));
  selectedHideBox_ = static_cast<int>(hideBoxes_.size()) - 1;
  hideBoxesVisible_ = true;
  ResetHideBoxGizmo();
  CommitHideBoxes();
}

void Application::ClearHideBoxes() {
  if (hideBoxes_.empty()) {
    return;
  }

  hideBoxes_.clear();
  selectedHideBox_ = -1;
  ResetHideBoxGizmo();
  CommitHideBoxes();
}

void Application::ClearPointSelections() {
  pointSelections_.clear();
  activePointSelection_ = -1;
  hoveredPointActive_ = false;
  hoverPickCache_.valid = false;
  pointScaleHandleHovered_ = false;
  pointScaleHandleHotSelection_ = -1;
  pointScaleDrag_ = {};
  pointContextMenuSelection_ = -1;
  pointContextMenuOpenRequested_ = false;
}

void Application::DeletePointSelection(int selectionIndex) {
  if (selectionIndex < 0 || selectionIndex >= static_cast<int>(pointSelections_.size())) {
    return;
  }

  pointSelections_.erase(pointSelections_.begin() + selectionIndex);
  if (activePointSelection_ == selectionIndex) {
    activePointSelection_ = -1;
  } else if (activePointSelection_ > selectionIndex) {
    --activePointSelection_;
  }

  if (pointScaleHandleHotSelection_ == selectionIndex) {
    pointScaleHandleHotSelection_ = -1;
  } else if (pointScaleHandleHotSelection_ > selectionIndex) {
    --pointScaleHandleHotSelection_;
  }

  if (pointScaleDrag_.selectionIndex == selectionIndex) {
    pointScaleDrag_ = {};
  } else if (pointScaleDrag_.selectionIndex > selectionIndex) {
    --pointScaleDrag_.selectionIndex;
  }

  pointContextMenuSelection_ = -1;
  hoverPickCache_.valid = false;
}

void Application::CommitHideBoxes() {
  if (isolatedSelectionWorkflowState_ != IsolatedSelectionWorkflowState::kDeleting) {
    ClearIsolatedSelectionPreview();
  }
  committedHideBoxes_ = hideBoxes_;
  hoverPickCache_.valid = false;
  renderer_.SetHideBoxes(committedHideBoxes_);
  if (committedHideBoxes_.empty()) {
    visiblePointCount_ = static_cast<std::uint64_t>(currentCloud_.points.size());
    visiblePointCountAccurate_ = true;
  } else {
    visiblePointCountAccurate_ = false;
  }
}

void Application::ResetHideBoxGizmo() {
  hideBoxGizmoHotAxis_ = HideBoxGizmoAxis::kNone;
  hideBoxGizmoHovered_ = false;
  hideBoxGizmoDrag_ = {};
}

}  // namespace pointmod
