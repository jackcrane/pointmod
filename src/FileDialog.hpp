#pragma once

#include <filesystem>
#include <optional>

namespace pointmod {

std::optional<std::filesystem::path> OpenPointCloudDialog();
std::optional<std::filesystem::path> SavePointCloudDialog(const std::filesystem::path& suggestedPath);

}  // namespace pointmod
