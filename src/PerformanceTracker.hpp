#pragma once

#include <array>
#include <chrono>
#include <cstdint>
#include <vector>

namespace pointmod {

class PerformanceTracker {
 public:
  enum class TaskId : std::size_t {
    kEventLoop = 0,
    kPointCloud,
    kUi,
    kHideBoxTools,
    kPointSelection,
    kCamera,
    kDeletionWorkflow,
    kIsolatedSelection,
    kFrameStats,
    kRenderer,
    kCount,
  };

  struct TaskRow {
    TaskId id = TaskId::kEventLoop;
    const char* label = "";
    float smoothedFrameMs = 0.0f;
    float frameContributionPercent = 0.0f;
    double memoryPercent = 0.0;
    std::uint64_t memoryBytes = 0;
  };

  class ScopedTask {
   public:
    ScopedTask() = default;
    ScopedTask(PerformanceTracker* tracker, TaskId id);
    ScopedTask(const ScopedTask&) = delete;
    ScopedTask& operator=(const ScopedTask&) = delete;
    ScopedTask(ScopedTask&& other) noexcept;
    ScopedTask& operator=(ScopedTask&& other) noexcept;
    ~ScopedTask();

   private:
    PerformanceTracker* tracker_ = nullptr;
    TaskId id_ = TaskId::kEventLoop;
    std::chrono::steady_clock::time_point startTime_{};
  };

  PerformanceTracker();

  void BeginFrame();
  void EndFrame();
  ScopedTask Measure(TaskId id);
  void SetTaskMemoryBytes(TaskId id, std::uint64_t bytes);
  [[nodiscard]] std::vector<TaskRow> BuildRows() const;
  [[nodiscard]] std::uint64_t TotalSystemMemoryBytes() const;
  [[nodiscard]] std::uint64_t AttributedMemoryBytes() const;
  [[nodiscard]] float SmoothedFrameMs() const;
  [[nodiscard]] static const char* TaskLabel(TaskId id);

 private:
  struct TaskStats {
    double frameMs = 0.0;
    float smoothedMs = 0.0f;
    std::uint64_t memoryBytes = 0;
  };

  void AddSample(TaskId id, double elapsedMs);

  std::array<TaskStats, static_cast<std::size_t>(TaskId::kCount)> taskStats_{};
  std::chrono::steady_clock::time_point frameStartTime_{};
  bool frameActive_ = false;
  float smoothedFrameMs_ = 0.0f;
  std::uint64_t totalSystemMemoryBytes_ = 0;
};

}  // namespace pointmod
