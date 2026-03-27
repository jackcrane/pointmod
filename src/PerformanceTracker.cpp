#include "PerformanceTracker.hpp"

#include <algorithm>
#include <utility>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#elif defined(__APPLE__)
#include <sys/sysctl.h>
#elif defined(__linux__)
#include <sys/sysinfo.h>
#endif

namespace pointmod {

namespace {

constexpr float kSmoothingFactor = 0.1f;

constexpr std::array<const char*, static_cast<std::size_t>(PerformanceTracker::TaskId::kCount)> kTaskLabels = {
  "Event loop",
  "Point cloud",
  "UI",
  "Hide box tools",
  "Point selection",
  "Camera",
  "Deletion workflow",
  "Isolated selection",
  "Frame stats",
  "Renderer",
};

std::uint64_t QueryTotalSystemMemoryBytes() {
#if defined(_WIN32)
  MEMORYSTATUSEX status = {};
  status.dwLength = sizeof(status);
  if (GlobalMemoryStatusEx(&status) == FALSE) {
    return 0;
  }
  return static_cast<std::uint64_t>(status.ullTotalPhys);
#elif defined(__APPLE__)
  std::uint64_t memoryBytes = 0;
  std::size_t size = sizeof(memoryBytes);
  if (sysctlbyname("hw.memsize", &memoryBytes, &size, nullptr, 0) != 0) {
    return 0;
  }
  return memoryBytes;
#elif defined(__linux__)
  struct sysinfo info = {};
  if (sysinfo(&info) != 0) {
    return 0;
  }
  return static_cast<std::uint64_t>(info.totalram) * static_cast<std::uint64_t>(info.mem_unit);
#else
  return 0;
#endif
}

}  // namespace

PerformanceTracker::ScopedTask::ScopedTask(PerformanceTracker* tracker, TaskId id)
    : tracker_(tracker),
      id_(id),
      startTime_(std::chrono::steady_clock::now()) {}

PerformanceTracker::ScopedTask::ScopedTask(ScopedTask&& other) noexcept
    : tracker_(std::exchange(other.tracker_, nullptr)),
      id_(other.id_),
      startTime_(other.startTime_) {}

PerformanceTracker::ScopedTask& PerformanceTracker::ScopedTask::operator=(ScopedTask&& other) noexcept {
  if (this == &other) {
    return *this;
  }

  if (tracker_ != nullptr) {
    const auto endTime = std::chrono::steady_clock::now();
    const double elapsedMs = std::chrono::duration<double, std::milli>(endTime - startTime_).count();
    tracker_->AddSample(id_, elapsedMs);
  }

  tracker_ = std::exchange(other.tracker_, nullptr);
  id_ = other.id_;
  startTime_ = other.startTime_;
  return *this;
}

PerformanceTracker::ScopedTask::~ScopedTask() {
  if (tracker_ == nullptr) {
    return;
  }

  const auto endTime = std::chrono::steady_clock::now();
  const double elapsedMs = std::chrono::duration<double, std::milli>(endTime - startTime_).count();
  tracker_->AddSample(id_, elapsedMs);
}

PerformanceTracker::PerformanceTracker()
    : totalSystemMemoryBytes_(QueryTotalSystemMemoryBytes()) {}

void PerformanceTracker::BeginFrame() {
  for (TaskStats& stats : taskStats_) {
    stats.frameMs = 0.0;
  }
  frameStartTime_ = std::chrono::steady_clock::now();
  frameActive_ = true;
}

void PerformanceTracker::EndFrame() {
  if (!frameActive_) {
    return;
  }

  frameActive_ = false;
  const auto endTime = std::chrono::steady_clock::now();
  const float frameMs = static_cast<float>(
    std::chrono::duration<double, std::milli>(endTime - frameStartTime_).count());
  if (frameMs > 0.0f) {
    if (smoothedFrameMs_ <= 0.0f) {
      smoothedFrameMs_ = frameMs;
    } else {
      smoothedFrameMs_ = smoothedFrameMs_ * (1.0f - kSmoothingFactor) + frameMs * kSmoothingFactor;
    }
  }

  for (TaskStats& stats : taskStats_) {
    const float sampleMs = static_cast<float>(stats.frameMs);
    if (stats.smoothedMs <= 0.0f) {
      stats.smoothedMs = sampleMs;
    } else {
      stats.smoothedMs = stats.smoothedMs * (1.0f - kSmoothingFactor) + sampleMs * kSmoothingFactor;
    }
    stats.frameMs = 0.0;
  }
}

PerformanceTracker::ScopedTask PerformanceTracker::Measure(TaskId id) {
  return ScopedTask(this, id);
}

void PerformanceTracker::SetTaskMemoryBytes(TaskId id, std::uint64_t bytes) {
  taskStats_[static_cast<std::size_t>(id)].memoryBytes = bytes;
}

std::vector<PerformanceTracker::TaskRow> PerformanceTracker::BuildRows() const {
  std::vector<TaskRow> rows;
  rows.reserve(taskStats_.size());

  for (std::size_t index = 0; index < taskStats_.size(); ++index) {
    const TaskId id = static_cast<TaskId>(index);
    const TaskStats& stats = taskStats_[index];
    const double memoryPercent = totalSystemMemoryBytes_ == 0
      ? 0.0
      : static_cast<double>(stats.memoryBytes) * 100.0 / static_cast<double>(totalSystemMemoryBytes_);
    const float frameContributionPercent = smoothedFrameMs_ <= 0.0f
      ? 0.0f
      : stats.smoothedMs * 100.0f / smoothedFrameMs_;
    rows.push_back(TaskRow{
      .id = id,
      .label = TaskLabel(id),
      .smoothedFrameMs = stats.smoothedMs,
      .frameContributionPercent = frameContributionPercent,
      .memoryPercent = memoryPercent,
      .memoryBytes = stats.memoryBytes,
    });
  }

  return rows;
}

std::uint64_t PerformanceTracker::TotalSystemMemoryBytes() const {
  return totalSystemMemoryBytes_;
}

std::uint64_t PerformanceTracker::AttributedMemoryBytes() const {
  std::uint64_t totalBytes = 0;
  for (const TaskStats& stats : taskStats_) {
    totalBytes += stats.memoryBytes;
  }
  return totalBytes;
}

float PerformanceTracker::SmoothedFrameMs() const {
  return smoothedFrameMs_;
}

const char* PerformanceTracker::TaskLabel(TaskId id) {
  return kTaskLabels[static_cast<std::size_t>(id)];
}

void PerformanceTracker::AddSample(TaskId id, double elapsedMs) {
  taskStats_[static_cast<std::size_t>(id)].frameMs += elapsedMs;
}

}  // namespace pointmod
