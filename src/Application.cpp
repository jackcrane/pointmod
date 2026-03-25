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
#include <stdexcept>
#include <string>

namespace pointmod {

namespace {

constexpr std::size_t kFullDetailPointBudget = 8'000'000;
constexpr std::size_t kBalancedDetailPointBudget = 30'000'000;

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

RenderDetail ChooseRenderDetail(
  RenderDetail currentDetail,
  std::size_t residentPointCount,
  bool loading,
  bool interactionActive,
  float smoothedFps) {
  if (interactionActive || loading) {
    return RenderDetail::kInteraction;
  }

  if (residentPointCount > kBalancedDetailPointBudget) {
    return RenderDetail::kBalanced;
  }

  if (currentDetail == RenderDetail::kInteraction) {
    if (residentPointCount > kFullDetailPointBudget) {
      return RenderDetail::kBalanced;
    }
    return smoothedFps >= 40.0f ? RenderDetail::kFull : RenderDetail::kBalanced;
  }

  if (currentDetail == RenderDetail::kBalanced) {
    if (smoothedFps > 50.0f && residentPointCount <= kFullDetailPointBudget) {
      return RenderDetail::kFull;
    }
    if (smoothedFps > 0.0f && smoothedFps < 20.0f) {
      return RenderDetail::kInteraction;
    }
    return RenderDetail::kBalanced;
  }

  if (residentPointCount > kFullDetailPointBudget || (smoothedFps > 0.0f && smoothedFps < 32.0f)) {
    return RenderDetail::kBalanced;
  }

  return RenderDetail::kFull;
}

}  // namespace

int Application::Run() {
  try {
    InitializeWindow();
    InitializeImGui();
    renderer_.Initialize();

    while (!glfwWindowShouldClose(window_)) {
      glfwPollEvents();
      UpdateCloudLoading();
      HandleCameraInput();
      UpdateFrameStats();

      BeginImGuiFrame();
      RenderUi();
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

  InstallNativeMenu([this]() { StartOpenDialog(); });
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

void Application::RenderUi() {
  const ImGuiViewport* viewport = ImGui::GetMainViewport();
  ImGui::SetNextWindowPos(
    ImVec2(viewport->WorkPos.x + 20.0f, viewport->WorkPos.y + 20.0f),
    ImGuiCond_Always);
  ImGui::SetNextWindowSize(ImVec2(390.0f, 240.0f), ImGuiCond_Always);
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
#if defined(__APPLE__)
    ImGui::TextUnformatted("Shortcut: Cmd+O or drag and drop");
#else
    ImGui::TextUnformatted("Shortcut: Ctrl+O or drag and drop");
#endif

    ImGui::TextWrapped("%s", statusMessage_.c_str());
    ImGui::Separator();

    if (currentCloud_.renderPointCount > 0) {
      const std::string fileName = currentCloud_.sourcePath.filename().string();
      ImGui::Text("File: %s", fileName.c_str());
      ImGui::Text("Source points: %llu", static_cast<unsigned long long>(currentCloud_.sourcePointCount));
      ImGui::Text("Rendered points: %llu", static_cast<unsigned long long>(currentCloud_.renderPointCount));
      ImGui::Text("Sampling: %s", currentCloud_.sampledRender ? "enabled" : "off");
      if (currentCloud_.sampledRender) {
        ImGui::Text("Sampling stride: 1 / %llu", static_cast<unsigned long long>(currentCloud_.samplingStride));
      }
      ImGui::Text("FPS: %.1f (%.2f ms)", smoothedFps_, smoothedFrameMs_);
      ImGui::Text(
        "Display detail: %s (%llu pts)",
        DetailLabel(activeRenderDetail_),
        static_cast<unsigned long long>(renderer_.DisplayPointCount(activeRenderDetail_)));
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
    ImGui::TextUnformatted("Controls: left drag orbit, right drag pan, wheel zoom.");
  }
  ImGui::End();
}

void Application::RenderScene() {
  int framebufferWidth = 0;
  int framebufferHeight = 0;
  glfwGetFramebufferSize(window_, &framebufferWidth, &framebufferHeight);

  glClearColor(0.05f, 0.07f, 0.10f, 1.0f);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  renderer_.Render(camera_, framebufferWidth, framebufferHeight, pointSize_, activeRenderDetail_);
}

void Application::UpdateCloudLoading() {
  for (PointCloudChunk& chunk : loader_.TakePendingChunks()) {
    renderer_.Append(chunk);
    currentCloud_.bounds = chunk.bounds;
    currentCloud_.sourcePointCount = chunk.sourcePointCount;
    currentCloud_.renderPointCount = chunk.renderedPointCount;
    currentCloud_.sampledRender = chunk.sampledRender;
    currentCloud_.samplingStride = chunk.samplingStride;

    if (!cameraFramed_ && currentCloud_.bounds.IsValid()) {
      camera_.Frame(currentCloud_.bounds);
      cameraFramed_ = true;
    }
  }

  if (std::optional<PointCloudData> loaded = loader_.TakeCompleted()) {
    currentCloud_.sourcePath = loaded->sourcePath;
    currentCloud_.bounds = loaded->bounds;
    currentCloud_.sourcePointCount = loaded->sourcePointCount;
    currentCloud_.renderPointCount = loaded->renderPointCount;
    currentCloud_.sampledRender = loaded->sampledRender;
    currentCloud_.samplingStride = loaded->samplingStride;
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

  const bool loading = loader_.Snapshot().loading;
  activeRenderDetail_ = ChooseRenderDetail(
    activeRenderDetail_,
    renderer_.PointCount(),
    loading,
    interactionActive_,
    smoothedFps_);
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
  interactionActive_ = false;

  if (!io.WantCaptureMouse && renderer_.HasCloud()) {
    if (glfwGetMouseButton(window_, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS) {
      camera_.Rotate(deltaX, deltaY);
      cameraTouched_ = true;
      interactionActive_ = true;
    }
    if (glfwGetMouseButton(window_, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS) {
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

void Application::StartOpenDialog() {
  if (std::optional<std::filesystem::path> selectedPath = OpenPointCloudDialog()) {
    OpenPointCloud(*selectedPath);
  }
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
  currentCloud_ = {};
  currentCloud_.sourcePath = path;
  cameraFramed_ = false;
  cameraTouched_ = false;
  interactionActive_ = false;
  activeRenderDetail_ = RenderDetail::kFull;
  loader_.Start(path);
  statusMessage_ = "Opening " + path.filename().string();
}

}  // namespace pointmod
