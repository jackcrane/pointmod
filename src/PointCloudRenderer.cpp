#include "PointCloudRenderer.hpp"

#include <OpenGL/gl3.h>

#include <cstddef>
#include <stdexcept>
#include <vector>

namespace pointmod {

namespace {

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

}  // namespace

PointCloudRenderer::~PointCloudRenderer() {
  Shutdown();
}

void PointCloudRenderer::Shutdown() {
  if (!initialized_) {
    return;
  }

  if (vbo_ != 0) {
    glDeleteBuffers(1, &vbo_);
    vbo_ = 0;
  }
  if (vao_ != 0) {
    glDeleteVertexArrays(1, &vao_);
    vao_ = 0;
  }
  if (program_ != 0) {
    glDeleteProgram(program_);
    program_ = 0;
  }

  pointCount_ = 0;
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

  glGenVertexArrays(1, &vao_);
  glGenBuffers(1, &vbo_);

  glBindVertexArray(vao_);
  glBindBuffer(GL_ARRAY_BUFFER, vbo_);
  glEnableVertexAttribArray(0);
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(PointVertex), reinterpret_cast<void*>(offsetof(PointVertex, x)));
  glEnableVertexAttribArray(1);
  glVertexAttribPointer(1, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(PointVertex), reinterpret_cast<void*>(offsetof(PointVertex, r)));
  glBindVertexArray(0);

  initialized_ = true;
}

void PointCloudRenderer::Upload(const PointCloudData& cloud) {
  Initialize();

  glBindBuffer(GL_ARRAY_BUFFER, vbo_);
  glBufferData(
    GL_ARRAY_BUFFER,
    static_cast<GLsizeiptr>(cloud.points.size() * sizeof(PointVertex)),
    cloud.points.empty() ? nullptr : cloud.points.data(),
    GL_STATIC_DRAW);
  glBindBuffer(GL_ARRAY_BUFFER, 0);

  pointCount_ = cloud.points.size();
  bounds_ = cloud.bounds;
  error_.clear();
}

void PointCloudRenderer::Render(const OrbitCamera& camera, int viewportWidth, int viewportHeight, float pointSize) const {
  if (!initialized_ || pointCount_ == 0 || viewportWidth <= 0 || viewportHeight <= 0) {
    return;
  }

  glViewport(0, 0, viewportWidth, viewportHeight);
  glEnable(GL_DEPTH_TEST);
  glEnable(GL_PROGRAM_POINT_SIZE);

  glUseProgram(program_);
  const Mat4 viewProjection = camera.ViewProjection(static_cast<float>(viewportWidth) / static_cast<float>(viewportHeight));
  const int matrixLocation = glGetUniformLocation(program_, "uViewProjection");
  const int pointSizeLocation = glGetUniformLocation(program_, "uPointSize");
  glUniformMatrix4fv(matrixLocation, 1, GL_FALSE, viewProjection.m);
  glUniform1f(pointSizeLocation, pointSize);

  glBindVertexArray(vao_);
  glDrawArrays(GL_POINTS, 0, static_cast<GLsizei>(pointCount_));
  glBindVertexArray(0);
  glUseProgram(0);
}

bool PointCloudRenderer::HasCloud() const {
  return pointCount_ > 0;
}

std::size_t PointCloudRenderer::PointCount() const {
  return pointCount_;
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
