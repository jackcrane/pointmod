#define GLFW_INCLUDE_NONE
#include "Application.hpp"

#include "CocoaMenu.hpp"
#include "FileDialog.hpp"

#include <GLFW/glfw3.h>
#if defined(__APPLE__)
#include <OpenGL/gl3.h>
#else
#define GL_GLEXT_PROTOTYPES
#include <GL/gl.h>
#include <GL/glext.h>
#endif
#include <imgui.h>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_opengl3.h>

#include <algorithm>
#include <stdexcept>
#include <string>

namespace pointmod {

int Application::Run() {
  try {
    InitializeWindow();
    InitializeImGui();
    renderer_.Initialize();

    while (!glfwWindowShouldClose(window_)) {
      glfwPollEvents();
      UpdateCloudLoading();
      HandleCameraInput();

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
  glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
  glfwWindowHint(GLFW_COCOA_RETINA_FRAMEBUFFER, GLFW_TRUE);

  window_ = glfwCreateWindow(1600, 1000, "pointmod", nullptr, nullptr);
  if (window_ == nullptr) {
    throw std::runtime_error("Failed to create application window.");
  }

  glfwMakeContextCurrent(window_);
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
  ImGui::SetNextWindowSize(ImVec2(390.0f, 220.0f), ImGuiCond_Always);
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

    if (currentCloud_.previewPointCount > 0) {
      const std::string fileName = currentCloud_.sourcePath.filename().string();
      ImGui::Text("File: %s", fileName.c_str());
      ImGui::Text("Source points: %llu", static_cast<unsigned long long>(currentCloud_.sourcePointCount));
      ImGui::Text("Preview points: %llu", static_cast<unsigned long long>(currentCloud_.previewPointCount));
      ImGui::Text("Sampled preview: %s", currentCloud_.sampledPreview ? "yes" : "no");
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
      ImGui::Text("Preview: %llu points", static_cast<unsigned long long>(loaderState.progress.pointsKept));
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

  renderer_.Render(camera_, framebufferWidth, framebufferHeight, pointSize_);
}

void Application::UpdateCloudLoading() {
  if (std::optional<PointCloudData> loaded = loader_.TakeCompleted()) {
    currentCloud_ = std::move(*loaded);
    renderer_.Upload(currentCloud_);
    camera_.Frame(currentCloud_.bounds);
    statusMessage_ = "Loaded " + currentCloud_.sourcePath.filename().string();
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

  if (!io.WantCaptureMouse && renderer_.HasCloud()) {
    if (glfwGetMouseButton(window_, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS) {
      camera_.Rotate(deltaX, deltaY);
    }
    if (glfwGetMouseButton(window_, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS) {
      camera_.Pan(deltaX, deltaY);
    }
    if (pendingScrollY_ != 0.0f) {
      camera_.Zoom(pendingScrollY_);
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

  loader_.Start(path);
  statusMessage_ = "Opening " + path.filename().string();
}

}  // namespace pointmod
