#pragma once

#include "PointCloud.hpp"

#include <functional>
#include <stop_token>
#include <string>

namespace pointmod {

struct PlyLoadOptions {
  std::uint64_t maxRenderPoints = 80'000'000;
  std::size_t streamBatchPoints = 1'000'000;
};

struct PlyLoadProgress {
  std::uint64_t bytesRead = 0;
  std::uint64_t totalBytes = 0;
  std::uint64_t pointsRead = 0;
  std::uint64_t pointsKept = 0;
  std::string status;
};

struct PlySaveProgress {
  std::uint64_t pointsWritten = 0;
  std::uint64_t totalPoints = 0;
  std::string status;
};

using PlyLoadProgressCallback = std::function<void(const PlyLoadProgress&)>;
using PlySaveProgressCallback = std::function<void(const PlySaveProgress&)>;
using PlyPointChunkCallback = std::function<void(PointCloudChunk&&)>;

PointCloudData LoadPly(
  const std::filesystem::path& path,
  const PlyLoadOptions& options,
  std::stop_token stopToken,
  const PlyLoadProgressCallback& onProgress,
  const PlyPointChunkCallback& onChunk);
void SaveAsciiPly(
  const std::filesystem::path& path,
  const std::vector<PointVertex>& points,
  const PlySaveProgressCallback& onProgress = {});

}  // namespace pointmod
