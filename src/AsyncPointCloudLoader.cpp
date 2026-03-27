#include "AsyncPointCloudLoader.hpp"

#include <exception>
#include <utility>
#include <vector>

namespace pointmod {

AsyncPointCloudLoader::AsyncPointCloudLoader(PlyLoadOptions options)
    : options_(options) {}

AsyncPointCloudLoader::~AsyncPointCloudLoader() {
  if (worker_.joinable()) {
    worker_.request_stop();
    worker_.join();
  }
}

void AsyncPointCloudLoader::Start(const std::filesystem::path& path) {
  if (worker_.joinable()) {
    worker_.request_stop();
    worker_.join();
  }

  {
    std::scoped_lock lock(mutex_);
    completed_.reset();
    pendingChunks_.clear();
    state_ = State{
      .loading = true,
      .hasError = false,
      .path = path,
      .message = "Queued",
      .progress = {},
    };
  }

  worker_ = std::jthread([this, path](std::stop_token stopToken) {
    try {
      PointCloudData cloud = LoadPly(
        path,
        options_,
        stopToken,
        [this, path](const PlyLoadProgress& progress) {
          std::scoped_lock lock(mutex_);
          state_.loading = true;
          state_.hasError = false;
          state_.path = path;
          state_.message = progress.status;
          state_.progress = progress;
        },
        [this](PointCloudChunk&& chunk) {
          std::scoped_lock lock(mutex_);
          pendingChunks_.push_back(std::move(chunk));
        });

      std::scoped_lock lock(mutex_);
      completed_ = std::move(cloud);
      state_.loading = false;
      state_.hasError = false;
      state_.message = "Loaded";
    } catch (const std::exception& exception) {
      std::scoped_lock lock(mutex_);
      state_.loading = false;
      state_.hasError = true;
      state_.path = path;
      state_.message = exception.what();
    }
  });
}

AsyncPointCloudLoader::State AsyncPointCloudLoader::Snapshot() const {
  std::scoped_lock lock(mutex_);
  return state_;
}

std::vector<PointCloudChunk> AsyncPointCloudLoader::TakePendingChunks() {
  std::scoped_lock lock(mutex_);
  std::vector<PointCloudChunk> result;
  result.reserve(pendingChunks_.size());
  while (!pendingChunks_.empty()) {
    result.push_back(std::move(pendingChunks_.front()));
    pendingChunks_.pop_front();
  }
  return result;
}

std::optional<PointCloudData> AsyncPointCloudLoader::TakeCompleted() {
  std::scoped_lock lock(mutex_);
  if (!completed_) {
    return std::nullopt;
  }

  std::optional<PointCloudData> result = std::move(completed_);
  completed_.reset();
  return result;
}

std::uint64_t AsyncPointCloudLoader::ApproximateResidentBytes() const {
  std::scoped_lock lock(mutex_);

  std::uint64_t bytes = 0;
  for (const PointCloudChunk& chunk : pendingChunks_) {
    bytes += static_cast<std::uint64_t>(chunk.points.capacity()) * sizeof(PointVertex);
  }
  if (completed_) {
    bytes += static_cast<std::uint64_t>(completed_->points.capacity()) * sizeof(PointVertex);
  }
  bytes += static_cast<std::uint64_t>(state_.message.capacity());
  return bytes;
}

}  // namespace pointmod
