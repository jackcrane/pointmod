#pragma once

#include "PointCloud.hpp"

#include <functional>
#include <stop_token>
#include <string>

namespace pointmod {

struct PlyLoadOptions {
  std::size_t maxPreviewPoints = 5'000'000;
};

struct PlyLoadProgress {
  std::uint64_t bytesRead = 0;
  std::uint64_t totalBytes = 0;
  std::uint64_t pointsRead = 0;
  std::uint64_t pointsKept = 0;
  std::string status;
};

using PlyLoadProgressCallback = std::function<void(const PlyLoadProgress&)>;

PointCloudData LoadAsciiPlyPreview(
  const std::filesystem::path& path,
  const PlyLoadOptions& options,
  std::stop_token stopToken,
  const PlyLoadProgressCallback& onProgress);

}  // namespace pointmod
