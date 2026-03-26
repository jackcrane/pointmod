#include "CocoaMenu.hpp"

namespace pointmod {

bool InstallNativeMenu(
  GLFWwindow*,
  const std::function<void()>&,
  const std::function<void()>&) {
  return false;
}

void UninstallNativeMenu(GLFWwindow*) {
}

}  // namespace pointmod
