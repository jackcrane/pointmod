#include "PointCloudRenderer.hpp"

#include "OpenGL.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <limits>
#include <stdexcept>
#include <vector>

namespace pointmod {

namespace {

constexpr std::size_t kUploadChunkPoints = 1'000'000;
constexpr int kHideBoxTextureWidth = 3;
constexpr float kDepthRangeEpsilon = 0.0001f;
constexpr int kSphereSegments = 32;
constexpr int kSphereLatitudeSegments = 8;
constexpr int kSphereLongitudeSegments = 12;
constexpr std::uint8_t kSelectionRed = 235;
constexpr std::uint8_t kHoverGreen = 165;
constexpr std::uint8_t kHoverBlue = 32;

constexpr const char* kVertexShaderSource = R"(
#version 150 core
in vec3 aPosition;
in vec4 aColor;

uniform mat4 uViewProjection;
uniform float uPointSize;
uniform int uColorMode;
uniform vec3 uCameraPosition;
uniform vec2 uDepthRange;
uniform float uDepthCurve[32];
uniform int uHideBoxCount;
uniform sampler2D uHideBoxTexture;

out vec4 vColor;
flat out int vHidden;

const int kPointColorModeSource = 0;
const int kPointColorModeDepth = 1;
const int kDepthCurveSampleCount = 32;

vec4 QuaternionConjugate(vec4 q) {
  return vec4(-q.xyz, q.w);
}

vec3 RotateByQuaternion(vec3 value, vec4 q) {
  vec3 t = 2.0 * cross(q.xyz, value);
  return value + q.w * t + cross(q.xyz, t);
}

float SampleDepthCurve(float depth) {
  float scaled = clamp(depth, 0.0, 1.0) * float(kDepthCurveSampleCount - 1);
  int leftIndex = int(floor(scaled));
  int rightIndex = min(leftIndex + 1, kDepthCurveSampleCount - 1);
  float fraction = scaled - float(leftIndex);
  return mix(uDepthCurve[leftIndex], uDepthCurve[rightIndex], fraction);
}

void main() {
  gl_Position = uViewProjection * vec4(aPosition, 1.0);
  gl_PointSize = uPointSize;
  vHidden = 0;

  if (uColorMode == kPointColorModeDepth) {
    float distanceFromCamera = distance(aPosition, uCameraPosition);
    float normalizedDepth = clamp(
      (distanceFromCamera - uDepthRange.x) / max(uDepthRange.y - uDepthRange.x, 0.0001),
      0.0,
      1.0);
    float intensity = SampleDepthCurve(normalizedDepth);
    vColor = vec4(vec3(intensity), aColor.a);
  } else {
    vColor = aColor;
  }

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

void AppendLine(
  std::vector<PointVertex>& vertices,
  const Vec3& a,
  const Vec3& b,
  std::uint8_t r,
  std::uint8_t g,
  std::uint8_t bChannel) {
  vertices.push_back(PointVertex{a.x, a.y, a.z, r, g, bChannel, 255});
  vertices.push_back(PointVertex{b.x, b.y, b.z, r, g, bChannel, 255});
}

void AppendTriangle(
  std::vector<PointVertex>& vertices,
  const Vec3& a,
  const Vec3& b,
  const Vec3& c,
  std::uint8_t r,
  std::uint8_t g,
  std::uint8_t bChannel,
  std::uint8_t alpha) {
  vertices.push_back(PointVertex{a.x, a.y, a.z, r, g, bChannel, alpha});
  vertices.push_back(PointVertex{b.x, b.y, b.z, r, g, bChannel, alpha});
  vertices.push_back(PointVertex{c.x, c.y, c.z, r, g, bChannel, alpha});
}

void AppendCircle(
  std::vector<PointVertex>& vertices,
  const Vec3& center,
  float radius,
  const Vec3& axisA,
  const Vec3& axisB,
  std::uint8_t r,
  std::uint8_t g,
  std::uint8_t bChannel) {
  for (int segmentIndex = 0; segmentIndex < kSphereSegments; ++segmentIndex) {
    const float angleA = static_cast<float>(segmentIndex) / static_cast<float>(kSphereSegments) * 6.28318530718f;
    const float angleB = static_cast<float>(segmentIndex + 1) / static_cast<float>(kSphereSegments) * 6.28318530718f;
    const Vec3 pointA = center + axisA * (std::cos(angleA) * radius) + axisB * (std::sin(angleA) * radius);
    const Vec3 pointB = center + axisA * (std::cos(angleB) * radius) + axisB * (std::sin(angleB) * radius);
    AppendLine(vertices, pointA, pointB, r, g, bChannel);
  }
}

void AppendSelectionSphere(std::vector<PointVertex>& vertices, const SelectionSphere& sphere) {
  const float radius = (std::max)(sphere.radius, 0.001f);
  AppendCircle(vertices, sphere.center, radius, {1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, kSelectionRed, 70, 70);
  AppendCircle(vertices, sphere.center, radius, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 1.0f}, kSelectionRed, 70, 70);
  AppendCircle(vertices, sphere.center, radius, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f, 1.0f}, kSelectionRed, 70, 70);
}

Vec3 SpherePoint(const SelectionSphere& sphere, float theta, float phi) {
  const float sinTheta = std::sin(theta);
  return {
    sphere.center.x + sphere.radius * sinTheta * std::cos(phi),
    sphere.center.y + sphere.radius * sinTheta * std::sin(phi),
    sphere.center.z + sphere.radius * std::cos(theta),
  };
}

void AppendSelectionSphereSurface(std::vector<PointVertex>& vertices, const SelectionSphere& sphere) {
  const float radius = (std::max)(sphere.radius, 0.001f);
  const SelectionSphere surfaceSphere{
    .center = sphere.center,
    .radius = radius,
  };
  for (int latitudeIndex = 0; latitudeIndex < kSphereLatitudeSegments; ++latitudeIndex) {
    const float thetaA = static_cast<float>(latitudeIndex) / static_cast<float>(kSphereLatitudeSegments) * 3.14159265359f;
    const float thetaB = static_cast<float>(latitudeIndex + 1) / static_cast<float>(kSphereLatitudeSegments) * 3.14159265359f;
    for (int longitudeIndex = 0; longitudeIndex < kSphereLongitudeSegments; ++longitudeIndex) {
      const float phiA = static_cast<float>(longitudeIndex) / static_cast<float>(kSphereLongitudeSegments) * 6.28318530718f;
      const float phiB = static_cast<float>(longitudeIndex + 1) / static_cast<float>(kSphereLongitudeSegments) * 6.28318530718f;
      const Vec3 a = SpherePoint(surfaceSphere, thetaA, phiA);
      const Vec3 b = SpherePoint(surfaceSphere, thetaB, phiA);
      const Vec3 c = SpherePoint(surfaceSphere, thetaB, phiB);
      const Vec3 d = SpherePoint(surfaceSphere, thetaA, phiB);
      AppendTriangle(vertices, a, b, c, kSelectionRed, 70, 70, 128);
      AppendTriangle(vertices, a, c, d, kSelectionRed, 70, 70, 128);
    }
  }
}

float DistanceToBounds(const Bounds& bounds, const Vec3& point) {
  const float dx = (std::max)((std::max)(bounds.min.x - point.x, 0.0f), point.x - bounds.max.x);
  const float dy = (std::max)((std::max)(bounds.min.y - point.y, 0.0f), point.y - bounds.max.y);
  const float dz = (std::max)((std::max)(bounds.min.z - point.z, 0.0f), point.z - bounds.max.z);
  return std::sqrt(dx * dx + dy * dy + dz * dz);
}

std::array<float, 2> ComputeDepthRange(const Bounds& bounds, const OrbitCamera& camera) {
  if (!bounds.IsValid()) {
    return {0.0f, 1.0f};
  }

  const Vec3 eye = camera.Position();
  const float minimumDistance = DistanceToBounds(bounds, eye);
  const Vec3 corners[8] = {
    {bounds.min.x, bounds.min.y, bounds.min.z},
    {bounds.max.x, bounds.min.y, bounds.min.z},
    {bounds.min.x, bounds.max.y, bounds.min.z},
    {bounds.max.x, bounds.max.y, bounds.min.z},
    {bounds.min.x, bounds.min.y, bounds.max.z},
    {bounds.max.x, bounds.min.y, bounds.max.z},
    {bounds.min.x, bounds.max.y, bounds.max.z},
    {bounds.max.x, bounds.max.y, bounds.max.z},
  };

  float maximumDistance = minimumDistance;
  for (const Vec3& corner : corners) {
    maximumDistance = (std::max)(maximumDistance, Length(corner - eye));
  }

  if (maximumDistance - minimumDistance < kDepthRangeEpsilon) {
    return {minimumDistance, minimumDistance + 1.0f};
  }
  return {minimumDistance, maximumDistance};
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
  colorModeLocation_ = -1;
  cameraPositionLocation_ = -1;
  depthRangeLocation_ = -1;
  depthCurveLocation_ = -1;
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
  colorModeLocation_ = glGetUniformLocation(program_, "uColorMode");
  cameraPositionLocation_ = glGetUniformLocation(program_, "uCameraPosition");
  depthRangeLocation_ = glGetUniformLocation(program_, "uDepthRange");
  depthCurveLocation_ = glGetUniformLocation(program_, "uDepthCurve[0]");
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
  PointColorMode colorMode,
  const std::array<float, kDepthColorCurveSampleCount>& depthCurve,
  RenderDetail detail,
  float interactionPointFraction,
  const std::vector<HideBox>& displayHideBoxes,
  bool drawHideBoxes,
  int selectedHideBox,
  const std::vector<SelectionSphere>& selectionSpheres,
  bool drawHoveredPoint,
  const Vec3& hoveredPoint) const {
  if (!initialized_ || viewportWidth <= 0 || viewportHeight <= 0) {
    return;
  }

  if (
    pointCount_ == 0 &&
    (!drawHideBoxes || displayHideBoxes.empty()) &&
    selectionSpheres.empty() &&
    !drawHoveredPoint) {
    return;
  }

  glViewport(0, 0, viewportWidth, viewportHeight);
  glEnable(GL_DEPTH_TEST);
  glEnable(GL_PROGRAM_POINT_SIZE);

  glUseProgram(program_);
  const Mat4 viewProjection = camera.ViewProjection(static_cast<float>(viewportWidth) / static_cast<float>(viewportHeight));
  const Vec3 eye = camera.Position();
  const std::array<float, 2> depthRange = ComputeDepthRange(bounds_, camera);
  glUniformMatrix4fv(viewProjectionLocation_, 1, GL_FALSE, viewProjection.m);
  glUniform1f(pointSizeLocation_, pointSize);
  glUniform1i(colorModeLocation_, static_cast<int>(colorMode));
  glUniform3f(cameraPositionLocation_, eye.x, eye.y, eye.z);
  glUniform2f(depthRangeLocation_, depthRange[0], depthRange[1]);
  glUniform1fv(depthCurveLocation_, static_cast<GLsizei>(depthCurve.size()), depthCurve.data());
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
  if (!selectionSpheres.empty() || drawHoveredPoint) {
    RenderSelectionOverlay(viewProjection, pointSize, selectionSpheres, drawHoveredPoint, hoveredPoint);
  }

  glBindTexture(GL_TEXTURE_2D, 0);
  glBindVertexArray(0);
  glDisable(GL_BLEND);
  glDepthMask(GL_TRUE);
  glDisable(GL_DEPTH_TEST);
  glDisable(GL_PROGRAM_POINT_SIZE);
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
  glUniform1i(colorModeLocation_, static_cast<int>(PointColorMode::kSource));
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

void PointCloudRenderer::RenderSelectionOverlay(
  const Mat4& viewProjection,
  float pointSize,
  const std::vector<SelectionSphere>& selectionSpheres,
  bool drawHoveredPoint,
  const Vec3& hoveredPoint) const {
  std::vector<PointVertex> surfaceVertices;
  surfaceVertices.reserve(selectionSpheres.size() * static_cast<std::size_t>(kSphereLatitudeSegments * kSphereLongitudeSegments * 6));
  for (const SelectionSphere& sphere : selectionSpheres) {
    AppendSelectionSphereSurface(surfaceVertices, sphere);
  }

  std::vector<PointVertex> lineVertices;
  lineVertices.reserve(selectionSpheres.size() * static_cast<std::size_t>(kSphereSegments * 3 * 2));
  for (const SelectionSphere& sphere : selectionSpheres) {
    AppendSelectionSphere(lineVertices, sphere);
  }

  glUniformMatrix4fv(viewProjectionLocation_, 1, GL_FALSE, viewProjection.m);
  glUniform1i(colorModeLocation_, static_cast<int>(PointColorMode::kSource));
  glUniform1i(hideBoxCountLocation_, 0);

  if (!surfaceVertices.empty()) {
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_FALSE);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glUniform1f(pointSizeLocation_, 1.0f);
    glBindVertexArray(overlayVao_);
    glBindBuffer(GL_ARRAY_BUFFER, overlayVbo_);
    glBufferData(
      GL_ARRAY_BUFFER,
      static_cast<GLsizeiptr>(surfaceVertices.size() * sizeof(PointVertex)),
      surfaceVertices.data(),
      GL_DYNAMIC_DRAW);
    glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(surfaceVertices.size()));
    glDisable(GL_CULL_FACE);
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
  }

  if (!lineVertices.empty()) {
    glDisable(GL_DEPTH_TEST);
    glUniform1f(pointSizeLocation_, 1.0f);
    glBindVertexArray(overlayVao_);
    glBindBuffer(GL_ARRAY_BUFFER, overlayVbo_);
    glBufferData(
      GL_ARRAY_BUFFER,
      static_cast<GLsizeiptr>(lineVertices.size() * sizeof(PointVertex)),
      lineVertices.data(),
      GL_DYNAMIC_DRAW);
    glDrawArrays(GL_LINES, 0, static_cast<GLsizei>(lineVertices.size()));
    glEnable(GL_DEPTH_TEST);
  }

  std::vector<PointVertex> pointVertices;
  pointVertices.reserve(selectionSpheres.size() + (drawHoveredPoint ? 1U : 0U));
  for (const SelectionSphere& sphere : selectionSpheres) {
    pointVertices.push_back(PointVertex{
      sphere.center.x,
      sphere.center.y,
      sphere.center.z,
      kSelectionRed,
      70,
      70,
      255,
    });
  }
  if (drawHoveredPoint) {
    pointVertices.push_back(PointVertex{
      hoveredPoint.x,
      hoveredPoint.y,
      hoveredPoint.z,
      255,
      kHoverGreen,
      kHoverBlue,
      255,
    });
  }

  if (!pointVertices.empty()) {
    glDisable(GL_DEPTH_TEST);
    glUniform1f(pointSizeLocation_, (std::max)(pointSize + 4.0f, 6.0f));
    glBindVertexArray(overlayVao_);
    glBindBuffer(GL_ARRAY_BUFFER, overlayVbo_);
    glBufferData(
      GL_ARRAY_BUFFER,
      static_cast<GLsizeiptr>(pointVertices.size() * sizeof(PointVertex)),
      pointVertices.data(),
      GL_DYNAMIC_DRAW);
    glDrawArrays(GL_POINTS, 0, static_cast<GLsizei>(pointVertices.size()));
    glEnable(GL_DEPTH_TEST);
  }
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
