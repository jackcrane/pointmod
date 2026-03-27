#pragma once

#include <functional>

struct GLFWwindow;

namespace pointmod {

bool InstallNativeMenu(
  GLFWwindow* window,
  const std::function<void()>& onOpenRequested,
  const std::function<void()>& onSaveRequested,
  const std::function<void()>& onResetViewRequested,
  const std::function<void()>& onTaskManagerRequested);
void UninstallNativeMenu(GLFWwindow* window);

}  // namespace pointmod
