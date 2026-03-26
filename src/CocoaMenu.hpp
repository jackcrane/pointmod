#pragma once

#include <functional>

namespace pointmod {

void InstallNativeMenu(
  const std::function<void()>& onOpenRequested,
  const std::function<void()>& onResetViewRequested);

}  // namespace pointmod
