#pragma once

#include <filesystem>
#include <optional>

namespace pointmod {

std::optional<std::filesystem::path> OpenPointCloudDialog();

}  // namespace pointmod
