#include "PointCloudRenderer.hpp"

#if defined(__APPLE__)
#include <OpenGL/gl3.h>
#else
#define GL_GLEXT_PROTOTYPES
#include <GL/gl.h>
#include <GL/glext.h>
#endif

#include <cstddef>
#include <stdexcept>
#include <vector>

namespace pointmod {

namespace {

constexpr std::size_t kTargetBalancedChunkPoints = 64'000;
constexpr std::size_t kTargetInteractionChunkPoints = 16'000;

constexpr const char* kVertexShaderSource = R"(
#version 150 core
in vec3 aPosition;
in vec4 aColor;

uniform mat4 uViewProjection;
uniform float uPointSize;

out vec4 vColor;

void main() {
  gl_Position = uViewProjection * vec4(aPosition, 1.0);
  gl_PointSize = uPointSize;
  vColor = aColor;
}
)";

constexpr const char* kFragmentShaderSource = R"(
#version 150 core
in vec4 vColor;
out vec4 fragColor;

void main() {
  fragColor = vColor;
}
)";

void UploadBuffer(
  unsigned int& vao,
  unsigned int& vbo,
  const std::vector<PointVertex>& points) {
  glGenVertexArrays(1, &vao);
  glGenBuffers(1, &vbo);

  glBindVertexArray(vao);
  glBindBuffer(GL_ARRAY_BUFFER, vbo);
  glBufferData(
    GL_ARRAY_BUFFER,
    static_cast<GLsizeiptr>(points.size() * sizeof(PointVertex)),
    points.data(),
    GL_STATIC_DRAW);
  glEnableVertexAttribArray(0);
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(PointVertex), reinterpret_cast<void*>(offsetof(PointVertex, x)));
  glEnableVertexAttribArray(1);
  glVertexAttribPointer(1, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(PointVertex), reinterpret_cast<void*>(offsetof(PointVertex, r)));
  glBindBuffer(GL_ARRAY_BUFFER, 0);
  glBindVertexArray(0);
}

std::vector<PointVertex> BuildReducedPoints(const std::vector<PointVertex>& points, std::size_t targetPointCount) {
  if (points.size() <= targetPointCount || targetPointCount == 0) {
    return {};
  }

  const std::size_t stride = std::max<std::size_t>(1, (points.size() + targetPointCount - 1) / targetPointCount);
  std::vector<PointVertex> reducedPoints;
  reducedPoints.reserve((points.size() + stride - 1) / stride);
  for (std::size_t pointIndex = 0; pointIndex < points.size(); pointIndex += stride) {
    reducedPoints.push_back(points[pointIndex]);
  }
  return reducedPoints;
}

}  // namespace

PointCloudRenderer::~PointCloudRenderer() {
  Shutdown();
}

void PointCloudRenderer::Shutdown() {
  if (!initialized_) {
    return;
  }

  Clear();

  if (program_ != 0) {
    glDeleteProgram(program_);
    program_ = 0;
  }

  viewProjectionLocation_ = -1;
  pointSizeLocation_ = -1;
  initialized_ = false;
}

void PointCloudRenderer::Initialize() {
  if (initialized_) {
    return;
  }

  const unsigned int vertexShader = CompileShader(GL_VERTEX_SHADER, kVertexShaderSource);
  const unsigned int fragmentShader = CompileShader(GL_FRAGMENT_SHADER, kFragmentShaderSource);

  program_ = glCreateProgram();
  glAttachShader(program_, vertexShader);
  glAttachShader(program_, fragmentShader);
  glBindAttribLocation(program_, 0, "aPosition");
  glBindAttribLocation(program_, 1, "aColor");
  glLinkProgram(program_);

  int linked = 0;
  glGetProgramiv(program_, GL_LINK_STATUS, &linked);
  glDeleteShader(vertexShader);
  glDeleteShader(fragmentShader);

  if (linked != GL_TRUE) {
    int logLength = 0;
    glGetProgramiv(program_, GL_INFO_LOG_LENGTH, &logLength);
    std::vector<char> infoLog(static_cast<std::size_t>(logLength));
    glGetProgramInfoLog(program_, logLength, nullptr, infoLog.data());
    error_ = std::string(infoLog.begin(), infoLog.end());
    throw std::runtime_error("Failed to link point cloud shader program.");
  }

  viewProjectionLocation_ = glGetUniformLocation(program_, "uViewProjection");
  pointSizeLocation_ = glGetUniformLocation(program_, "uPointSize");
  initialized_ = true;
}

void PointCloudRenderer::Clear() {
  for (GpuChunk& chunk : chunks_) {
    if (chunk.vbo != 0) {
      glDeleteBuffers(1, &chunk.vbo);
      chunk.vbo = 0;
    }
    if (chunk.vao != 0) {
      glDeleteVertexArrays(1, &chunk.vao);
      chunk.vao = 0;
    }
    if (chunk.balancedVbo != 0) {
      glDeleteBuffers(1, &chunk.balancedVbo);
      chunk.balancedVbo = 0;
    }
    if (chunk.balancedVao != 0) {
      glDeleteVertexArrays(1, &chunk.balancedVao);
      chunk.balancedVao = 0;
    }
    if (chunk.interactionVbo != 0) {
      glDeleteBuffers(1, &chunk.interactionVbo);
      chunk.interactionVbo = 0;
    }
    if (chunk.interactionVao != 0) {
      glDeleteVertexArrays(1, &chunk.interactionVao);
      chunk.interactionVao = 0;
    }
  }

  chunks_.clear();
  pointCount_ = 0;
  bounds_ = {};
  error_.clear();
}

void PointCloudRenderer::Append(const PointCloudChunk& chunk) {
  Initialize();

  if (chunk.points.empty()) {
    bounds_ = chunk.bounds;
    return;
  }

  GpuChunk gpuChunk;
  gpuChunk.pointCount = chunk.points.size();
  UploadBuffer(gpuChunk.vao, gpuChunk.vbo, chunk.points);

  std::vector<PointVertex> balancedPoints = BuildReducedPoints(chunk.points, kTargetBalancedChunkPoints);
  if (!balancedPoints.empty()) {
    gpuChunk.balancedPointCount = balancedPoints.size();
    UploadBuffer(gpuChunk.balancedVao, gpuChunk.balancedVbo, balancedPoints);
  }

  std::vector<PointVertex> interactionPoints = BuildReducedPoints(
    !balancedPoints.empty() ? balancedPoints : chunk.points,
    kTargetInteractionChunkPoints);
  if (!interactionPoints.empty()) {
    gpuChunk.interactionPointCount = interactionPoints.size();
    UploadBuffer(gpuChunk.interactionVao, gpuChunk.interactionVbo, interactionPoints);
  }

  chunks_.push_back(gpuChunk);
  pointCount_ += gpuChunk.pointCount;
  bounds_ = chunk.bounds;
  error_.clear();
}

void PointCloudRenderer::Render(
  const OrbitCamera& camera,
  int viewportWidth,
  int viewportHeight,
  float pointSize,
  RenderDetail detail) const {
  if (!initialized_ || pointCount_ == 0 || viewportWidth <= 0 || viewportHeight <= 0) {
    return;
  }

  glViewport(0, 0, viewportWidth, viewportHeight);
  glEnable(GL_DEPTH_TEST);
  glEnable(GL_PROGRAM_POINT_SIZE);

  glUseProgram(program_);
  const Mat4 viewProjection = camera.ViewProjection(static_cast<float>(viewportWidth) / static_cast<float>(viewportHeight));
  glUniformMatrix4fv(viewProjectionLocation_, 1, GL_FALSE, viewProjection.m);
  glUniform1f(pointSizeLocation_, pointSize);

  for (const GpuChunk& chunk : chunks_) {
    unsigned int vao = chunk.vao;
    std::size_t pointCount = chunk.pointCount;
    if (detail == RenderDetail::kInteraction && chunk.interactionPointCount > 0) {
      vao = chunk.interactionVao;
      pointCount = chunk.interactionPointCount;
    } else if (detail == RenderDetail::kInteraction && chunk.balancedPointCount > 0) {
      vao = chunk.balancedVao;
      pointCount = chunk.balancedPointCount;
    } else if (detail == RenderDetail::kBalanced && chunk.balancedPointCount > 0) {
      vao = chunk.balancedVao;
      pointCount = chunk.balancedPointCount;
    }
    glBindVertexArray(vao);
    glDrawArrays(GL_POINTS, 0, static_cast<GLsizei>(pointCount));
  }

  glBindVertexArray(0);
  glUseProgram(0);
}

bool PointCloudRenderer::HasCloud() const {
  return pointCount_ > 0;
}

std::size_t PointCloudRenderer::PointCount() const {
  return pointCount_;
}

std::size_t PointCloudRenderer::DisplayPointCount(RenderDetail detail) const {
  std::size_t displayPointCount = 0;
  for (const GpuChunk& chunk : chunks_) {
    if (detail == RenderDetail::kInteraction && chunk.interactionPointCount > 0) {
      displayPointCount += chunk.interactionPointCount;
    } else if (detail == RenderDetail::kInteraction && chunk.balancedPointCount > 0) {
      displayPointCount += chunk.balancedPointCount;
    } else if (detail == RenderDetail::kBalanced && chunk.balancedPointCount > 0) {
      displayPointCount += chunk.balancedPointCount;
    } else {
      displayPointCount += chunk.pointCount;
    }
  }
  return displayPointCount;
}

const Bounds& PointCloudRenderer::CurrentBounds() const {
  return bounds_;
}

std::string PointCloudRenderer::Error() const {
  return error_;
}

unsigned int PointCloudRenderer::CompileShader(unsigned int type, const char* source) {
  const unsigned int shader = glCreateShader(type);
  glShaderSource(shader, 1, &source, nullptr);
  glCompileShader(shader);

  int compiled = 0;
  glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
  if (compiled == GL_TRUE) {
    return shader;
  }

  int logLength = 0;
  glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &logLength);
  std::vector<char> infoLog(static_cast<std::size_t>(logLength));
  glGetShaderInfoLog(shader, logLength, nullptr, infoLog.data());
  error_ = std::string(infoLog.begin(), infoLog.end());
  glDeleteShader(shader);
  throw std::runtime_error("Failed to compile point cloud shader.");
}

}  // namespace pointmod
