#include "AsyncPointCloudLoader.hpp"

#include <exception>
#include <utility>

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
      PointCloudData cloud = LoadAsciiPlyPreview(
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

std::optional<PointCloudData> AsyncPointCloudLoader::TakeCompleted() {
  std::scoped_lock lock(mutex_);
  if (!completed_) {
    return std::nullopt;
  }

  std::optional<PointCloudData> result = std::move(completed_);
  completed_.reset();
  return result;
}

}  // namespace pointmod
