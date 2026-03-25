#pragma once

#include "PlyAsciiLoader.hpp"

#include <mutex>
#include <optional>
#include <string>
#include <thread>

namespace pointmod {

class AsyncPointCloudLoader {
 public:
  struct State {
    bool loading = false;
    bool hasError = false;
    std::filesystem::path path;
    std::string message;
    PlyLoadProgress progress;
  };

  explicit AsyncPointCloudLoader(PlyLoadOptions options = {});
  ~AsyncPointCloudLoader();

  void Start(const std::filesystem::path& path);
  [[nodiscard]] State Snapshot() const;
  std::optional<PointCloudData> TakeCompleted();

 private:
  PlyLoadOptions options_;
  mutable std::mutex mutex_;
  State state_;
  std::optional<PointCloudData> completed_;
  std::jthread worker_;
};

}  // namespace pointmod
