#pragma once

#include <functional>

namespace pointmod {

void InstallNativeMenu(const std::function<void()>& onOpenRequested);

}  // namespace pointmod
