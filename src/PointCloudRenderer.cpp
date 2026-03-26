#include "PointCloudRenderer.hpp"

#include "OpenGL.hpp"

#include <algorithm>
#include <cstddef>
#include <limits>
#include <stdexcept>
#include <vector>

namespace pointmod {

namespace {

constexpr std::size_t kUploadChunkPoints = 1'000'000;
constexpr int kHideBoxTextureWidth = 3;

constexpr const char* kVertexShaderSource = R"(
#version 150 core
in vec3 aPosition;
in vec4 aColor;

uniform mat4 uViewProjection;
uniform float uPointSize;
uniform int uHideBoxCount;
uniform sampler2D uHideBoxTexture;

out vec4 vColor;
flat out int vHidden;

vec4 QuaternionConjugate(vec4 q) {
  return vec4(-q.xyz, q.w);
}

vec3 RotateByQuaternion(vec3 value, vec4 q) {
  vec3 t = 2.0 * cross(q.xyz, value);
  return value + q.w * t + cross(q.xyz, t);
}

void main() {
  gl_Position = uViewProjection * vec4(aPosition, 1.0);
  gl_PointSize = uPointSize;
  vColor = aColor;
  vHidden = 0;

  for (int hideBoxIndex = 0; hideBoxIndex < uHideBoxCount; ++hideBoxIndex) {
    vec4 packed0 = texelFetch(uHideBoxTexture, ivec2(0, hideBoxIndex), 0);
    vec4 packed1 = texelFetch(uHideBoxTexture, ivec2(1, hideBoxIndex), 0);
    vec4 packed2 = texelFetch(uHideBoxTexture, ivec2(2, hideBoxIndex), 0);

    vec3 center = packed0.xyz;
    vec3 halfSize = vec3(packed0.w, packed1.xy);
    vec4 quaternion = normalize(vec4(packed1.z, packed2.xyz));
    vec3 local = RotateByQuaternion(aPosition - center, QuaternionConjugate(quaternion));
    if (
      abs(local.x) <= halfSize.x &&
      abs(local.y) <= halfSize.y &&
      abs(local.z) <= halfSize.z) {
      vHidden = 1;
      break;
    }
  }
}
)";

constexpr const char* kFragmentShaderSource = R"(
#version 150 core
in vec4 vColor;
flat in int vHidden;
out vec4 fragColor;

void main() {
  if (vHidden != 0) {
    discard;
  }
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

std::size_t ReverseBits(std::size_t value, unsigned int bitCount) {
  std::size_t reversed = 0;
  for (unsigned int bitIndex = 0; bitIndex < bitCount; ++bitIndex) {
    reversed = (reversed << 1U) | (value & 1U);
    value >>= 1U;
  }
  return reversed;
}

std::vector<PointVertex> BuildProgressivePoints(const std::vector<PointVertex>& points) {
  if (points.size() <= 1) {
    return points;
  }

  std::size_t pointCapacity = 1;
  unsigned int bitCount = 0;
  while (pointCapacity < points.size() && bitCount < std::numeric_limits<std::size_t>::digits - 1) {
    pointCapacity <<= 1U;
    ++bitCount;
  }

  std::vector<PointVertex> progressivePoints;
  progressivePoints.reserve(points.size());
  for (std::size_t sampleIndex = 0; sampleIndex < pointCapacity && progressivePoints.size() < points.size(); ++sampleIndex) {
    const std::size_t pointIndex = ReverseBits(sampleIndex, bitCount);
    if (pointIndex < points.size()) {
      progressivePoints.push_back(points[pointIndex]);
    }
  }
  return progressivePoints;
}

std::size_t ScalePointCount(std::size_t pointCount, float fraction) {
  if (pointCount == 0) {
    return 0;
  }

  if (fraction >= 0.9999f) {
    return pointCount;
  }

  const float clampedFraction = (std::clamp)(fraction, 0.0f, 1.0f);
  const std::size_t scaledPointCount = static_cast<std::size_t>(static_cast<double>(pointCount) * static_cast<double>(clampedFraction));
  return (std::max)(std::size_t{1}, scaledPointCount);
}

void AppendBoxEdge(std::vector<PointVertex>& vertices, const Vec3& a, const Vec3& b, std::uint8_t g, std::uint8_t bChannel) {
  vertices.push_back(PointVertex{a.x, a.y, a.z, 70, g, bChannel, 255});
  vertices.push_back(PointVertex{b.x, b.y, b.z, 70, g, bChannel, 255});
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
  hideBoxCountLocation_ = -1;
  hideBoxTextureLocation_ = -1;
  hideBoxCount_ = 0;
  if (hideBoxTexture_ != 0) {
    glDeleteTextures(1, &hideBoxTexture_);
    hideBoxTexture_ = 0;
  }
  if (overlayVbo_ != 0) {
    glDeleteBuffers(1, &overlayVbo_);
    overlayVbo_ = 0;
  }
  if (overlayVao_ != 0) {
    glDeleteVertexArrays(1, &overlayVao_);
    overlayVao_ = 0;
  }
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
  hideBoxCountLocation_ = glGetUniformLocation(program_, "uHideBoxCount");
  hideBoxTextureLocation_ = glGetUniformLocation(program_, "uHideBoxTexture");

  glGenTextures(1, &hideBoxTexture_);
  glBindTexture(GL_TEXTURE_2D, hideBoxTexture_);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  const float zeroData[kHideBoxTextureWidth * 4] = {};
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, kHideBoxTextureWidth, 1, 0, GL_RGBA, GL_FLOAT, zeroData);
  glBindTexture(GL_TEXTURE_2D, 0);

  glGenVertexArrays(1, &overlayVao_);
  glGenBuffers(1, &overlayVbo_);
  glBindVertexArray(overlayVao_);
  glBindBuffer(GL_ARRAY_BUFFER, overlayVbo_);
  glBufferData(GL_ARRAY_BUFFER, 0, nullptr, GL_DYNAMIC_DRAW);
  glEnableVertexAttribArray(0);
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(PointVertex), reinterpret_cast<void*>(offsetof(PointVertex, x)));
  glEnableVertexAttribArray(1);
  glVertexAttribPointer(1, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(PointVertex), reinterpret_cast<void*>(offsetof(PointVertex, r)));
  glBindBuffer(GL_ARRAY_BUFFER, 0);
  glBindVertexArray(0);

  initialized_ = true;
}

void PointCloudRenderer::SetHideBoxes(const std::vector<HideBox>& hideBoxes) {
  Initialize();

  hideBoxCount_ = static_cast<int>(hideBoxes.size());
  std::vector<float> textureData(static_cast<std::size_t>((std::max)(hideBoxCount_, 1)) * kHideBoxTextureWidth * 4, 0.0f);
  for (std::size_t hideBoxIndex = 0; hideBoxIndex < hideBoxes.size(); ++hideBoxIndex) {
    const HideBox& hideBox = hideBoxes[hideBoxIndex];
    const Quaternion quaternion = QuaternionFromEulerXYZ(hideBox.rotationDegrees);
    const std::size_t base = hideBoxIndex * kHideBoxTextureWidth * 4;
    textureData[base + 0] = hideBox.center.x;
    textureData[base + 1] = hideBox.center.y;
    textureData[base + 2] = hideBox.center.z;
    textureData[base + 3] = hideBox.halfSize.x;
    textureData[base + 4] = hideBox.halfSize.y;
    textureData[base + 5] = hideBox.halfSize.z;
    textureData[base + 6] = quaternion.x;
    textureData[base + 7] = 0.0f;
    textureData[base + 8] = quaternion.y;
    textureData[base + 9] = quaternion.z;
    textureData[base + 10] = quaternion.w;
    textureData[base + 11] = 0.0f;
  }

  glBindTexture(GL_TEXTURE_2D, hideBoxTexture_);
  glTexImage2D(
    GL_TEXTURE_2D,
    0,
    GL_RGBA32F,
    kHideBoxTextureWidth,
    (std::max)(hideBoxCount_, 1),
    0,
    GL_RGBA,
    GL_FLOAT,
    textureData.data());
  glBindTexture(GL_TEXTURE_2D, 0);
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
  }

  chunks_.clear();
  pointCount_ = 0;
  bounds_ = {};
  error_.clear();
}

void PointCloudRenderer::SetPointCloud(const std::vector<PointVertex>& points, const Bounds& bounds) {
  Initialize();
  Clear();
  bounds_ = bounds;

  if (points.empty()) {
    return;
  }

  for (std::size_t start = 0; start < points.size(); start += kUploadChunkPoints) {
    PointCloudChunk chunk;
    const std::size_t count = (std::min)(kUploadChunkPoints, points.size() - start);
    chunk.points.insert(chunk.points.end(), points.begin() + static_cast<std::ptrdiff_t>(start), points.begin() + static_cast<std::ptrdiff_t>(start + count));
    chunk.bounds = bounds;
    Append(chunk);
  }
}

void PointCloudRenderer::Append(const PointCloudChunk& chunk) {
  Initialize();

  if (chunk.points.empty()) {
    bounds_ = chunk.bounds;
    return;
  }

  GpuChunk gpuChunk;
  std::vector<PointVertex> progressivePoints = BuildProgressivePoints(chunk.points);
  gpuChunk.pointCount = progressivePoints.size();
  UploadBuffer(gpuChunk.vao, gpuChunk.vbo, progressivePoints);

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
  RenderDetail detail,
  float interactionPointFraction,
  const std::vector<HideBox>& displayHideBoxes,
  bool drawHideBoxes,
  int selectedHideBox) const {
  if (!initialized_ || viewportWidth <= 0 || viewportHeight <= 0) {
    return;
  }

  if (pointCount_ == 0 && (!drawHideBoxes || displayHideBoxes.empty())) {
    return;
  }

  glViewport(0, 0, viewportWidth, viewportHeight);
  glEnable(GL_DEPTH_TEST);
  glEnable(GL_PROGRAM_POINT_SIZE);

  glUseProgram(program_);
  const Mat4 viewProjection = camera.ViewProjection(static_cast<float>(viewportWidth) / static_cast<float>(viewportHeight));
  glUniformMatrix4fv(viewProjectionLocation_, 1, GL_FALSE, viewProjection.m);
  glUniform1f(pointSizeLocation_, pointSize);
  glUniform1i(hideBoxCountLocation_, hideBoxCount_);
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, hideBoxTexture_);
  glUniform1i(hideBoxTextureLocation_, 0);

  if (pointCount_ > 0) {
    for (const GpuChunk& chunk : chunks_) {
      std::size_t pointCount = chunk.pointCount;
      if (detail == RenderDetail::kInteraction) {
        pointCount = ScalePointCount(chunk.pointCount, interactionPointFraction);
      }
      if (pointCount == 0) {
        continue;
      }
      glBindVertexArray(chunk.vao);
      glDrawArrays(GL_POINTS, 0, static_cast<GLsizei>(pointCount));
    }
  }

  if (drawHideBoxes && !displayHideBoxes.empty()) {
    RenderHideBoxes(viewProjection, displayHideBoxes, selectedHideBox);
  }

  glBindTexture(GL_TEXTURE_2D, 0);
  glBindVertexArray(0);
  glUseProgram(0);
}

void PointCloudRenderer::RenderHideBoxes(
  const Mat4& viewProjection,
  const std::vector<HideBox>& hideBoxes,
  int selectedHideBox) const {
  std::vector<PointVertex> lineVertices;
  lineVertices.reserve(hideBoxes.size() * 24);

  constexpr Vec3 kCornerSigns[8] = {
    {-1.0f, -1.0f, -1.0f},
    {1.0f, -1.0f, -1.0f},
    {-1.0f, 1.0f, -1.0f},
    {1.0f, 1.0f, -1.0f},
    {-1.0f, -1.0f, 1.0f},
    {1.0f, -1.0f, 1.0f},
    {-1.0f, 1.0f, 1.0f},
    {1.0f, 1.0f, 1.0f},
  };
  constexpr int kEdges[12][2] = {
    {0, 1}, {0, 2}, {1, 3}, {2, 3},
    {4, 5}, {4, 6}, {5, 7}, {6, 7},
    {0, 4}, {1, 5}, {2, 6}, {3, 7},
  };

  for (std::size_t hideBoxIndex = 0; hideBoxIndex < hideBoxes.size(); ++hideBoxIndex) {
    const HideBox& hideBox = hideBoxes[hideBoxIndex];
    const Mat4 transform = Multiply(Translation(hideBox.center), EulerRotationXYZ(hideBox.rotationDegrees));
    Vec3 corners[8];
    for (int cornerIndex = 0; cornerIndex < 8; ++cornerIndex) {
      corners[cornerIndex] = TransformPoint(transform, {
        kCornerSigns[cornerIndex].x * hideBox.halfSize.x,
        kCornerSigns[cornerIndex].y * hideBox.halfSize.y,
        kCornerSigns[cornerIndex].z * hideBox.halfSize.z,
      });
    }

    const bool selected = static_cast<int>(hideBoxIndex) == selectedHideBox;
    const std::uint8_t green = selected ? 220 : 150;
    const std::uint8_t blue = selected ? 255 : 235;
    for (const auto& edge : kEdges) {
      AppendBoxEdge(lineVertices, corners[edge[0]], corners[edge[1]], green, blue);
    }
  }

  if (lineVertices.empty()) {
    return;
  }

  glUniformMatrix4fv(viewProjectionLocation_, 1, GL_FALSE, viewProjection.m);
  glUniform1f(pointSizeLocation_, 1.0f);
  glBindVertexArray(overlayVao_);
  glBindBuffer(GL_ARRAY_BUFFER, overlayVbo_);
  glBufferData(
    GL_ARRAY_BUFFER,
    static_cast<GLsizeiptr>(lineVertices.size() * sizeof(PointVertex)),
    lineVertices.data(),
    GL_DYNAMIC_DRAW);
  glDrawArrays(GL_LINES, 0, static_cast<GLsizei>(lineVertices.size()));
  glBindBuffer(GL_ARRAY_BUFFER, 0);
}

bool PointCloudRenderer::HasCloud() const {
  return pointCount_ > 0;
}

std::size_t PointCloudRenderer::PointCount() const {
  return pointCount_;
}

std::size_t PointCloudRenderer::DisplayPointCount(RenderDetail detail, float interactionPointFraction) const {
  std::size_t displayPointCount = 0;
  for (const GpuChunk& chunk : chunks_) {
    displayPointCount += detail == RenderDetail::kInteraction
      ? ScalePointCount(chunk.pointCount, interactionPointFraction)
      : chunk.pointCount;
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
