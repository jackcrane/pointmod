#include "PlyAsciiLoader.hpp"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <ios>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace pointmod {

namespace {

struct HeaderInfo {
  std::uint64_t vertexCount = 0;
  std::vector<std::string> vertexProperties;
  std::uint64_t bytesConsumed = 0;
};

std::string Trim(std::string_view value) {
  std::size_t start = 0;
  while (start < value.size() && std::isspace(static_cast<unsigned char>(value[start])) != 0) {
    ++start;
  }

  std::size_t end = value.size();
  while (end > start && std::isspace(static_cast<unsigned char>(value[end - 1])) != 0) {
    --end;
  }

  return std::string(value.substr(start, end - start));
}

bool StartsWith(std::string_view value, std::string_view prefix) {
  return value.substr(0, prefix.size()) == prefix;
}

HeaderInfo ParseHeader(std::istream& input) {
  HeaderInfo info;
  std::string line;
  bool inVertexElement = false;
  bool sawFormat = false;
  bool headerStarted = false;

  while (std::getline(input, line)) {
    info.bytesConsumed += static_cast<std::uint64_t>(line.size() + 1);
    const std::string trimmed = Trim(line);

    if (!headerStarted) {
      if (trimmed != "ply") {
        throw std::runtime_error("File is not a PLY file.");
      }
      headerStarted = true;
      continue;
    }

    if (StartsWith(trimmed, "format ")) {
      sawFormat = true;
      if (trimmed.find("ascii") == std::string::npos) {
        throw std::runtime_error("Only ASCII PLY files are supported right now.");
      }
      continue;
    }

    if (StartsWith(trimmed, "element ")) {
      inVertexElement = false;

      std::istringstream stream(trimmed);
      std::string elementKeyword;
      std::string elementName;
      stream >> elementKeyword >> elementName;
      if (elementName == "vertex") {
        stream >> info.vertexCount;
        inVertexElement = true;
      }
      continue;
    }

    if (StartsWith(trimmed, "property ") && inVertexElement) {
      std::istringstream stream(trimmed);
      std::string propertyKeyword;
      std::string propertyType;
      std::string propertyName;
      stream >> propertyKeyword >> propertyType >> propertyName;
      if (propertyType == "list") {
        std::string ignoredCountType;
        std::string ignoredValueType;
        stream >> ignoredCountType >> ignoredValueType >> propertyName;
      }
      info.vertexProperties.push_back(propertyName);
      continue;
    }

    if (trimmed == "end_header") {
      if (!sawFormat) {
        throw std::runtime_error("PLY header is missing a format declaration.");
      }
      if (info.vertexCount == 0) {
        throw std::runtime_error("PLY file does not declare any vertices.");
      }
      return info;
    }
  }

  throw std::runtime_error("PLY header ended unexpectedly.");
}

std::optional<std::size_t> FindPropertyIndex(
  const std::vector<std::string>& properties,
  std::string_view propertyName) {
  for (std::size_t index = 0; index < properties.size(); ++index) {
    if (properties[index] == propertyName) {
      return index;
    }
  }
  return std::nullopt;
}

double ParseNextScalar(char*& cursor) {
  while (*cursor != '\0' && std::isspace(static_cast<unsigned char>(*cursor)) != 0) {
    ++cursor;
  }

  if (*cursor == '\0') {
    throw std::runtime_error("Unexpected end of vertex row.");
  }

  char* end = nullptr;
  const double value = std::strtod(cursor, &end);
  if (end == cursor) {
    throw std::runtime_error("Failed to parse a vertex property.");
  }
  cursor = end;
  return value;
}

PointVertex ParseVertex(
  std::string& line,
  std::size_t lastRelevantPropertyIndex,
  std::size_t xIndex,
  std::size_t yIndex,
  std::size_t zIndex,
  std::optional<std::size_t> redIndex,
  std::optional<std::size_t> greenIndex,
  std::optional<std::size_t> blueIndex,
  std::optional<std::size_t> alphaIndex) {
  PointVertex vertex{};
  char* cursor = line.data();

  for (std::size_t propertyIndex = 0; propertyIndex <= lastRelevantPropertyIndex; ++propertyIndex) {
    const double value = ParseNextScalar(cursor);

    if (propertyIndex == xIndex) {
      vertex.x = static_cast<float>(value);
    } else if (propertyIndex == yIndex) {
      vertex.y = static_cast<float>(value);
    } else if (propertyIndex == zIndex) {
      vertex.z = static_cast<float>(value);
    } else if (redIndex && propertyIndex == *redIndex) {
      vertex.r = static_cast<std::uint8_t>(std::clamp(value, 0.0, 255.0));
    } else if (greenIndex && propertyIndex == *greenIndex) {
      vertex.g = static_cast<std::uint8_t>(std::clamp(value, 0.0, 255.0));
    } else if (blueIndex && propertyIndex == *blueIndex) {
      vertex.b = static_cast<std::uint8_t>(std::clamp(value, 0.0, 255.0));
    } else if (alphaIndex && propertyIndex == *alphaIndex) {
      vertex.a = static_cast<std::uint8_t>(std::clamp(value, 0.0, 255.0));
    }
  }

  return vertex;
}

void ReportProgress(
  const PlyLoadProgressCallback& onProgress,
  std::uint64_t bytesRead,
  std::uint64_t totalBytes,
  std::uint64_t pointsRead,
  std::uint64_t pointsKept,
  std::string status) {
  if (!onProgress) {
    return;
  }

  onProgress(PlyLoadProgress{
    .bytesRead = bytesRead,
    .totalBytes = totalBytes,
    .pointsRead = pointsRead,
    .pointsKept = pointsKept,
    .status = std::move(status),
  });
}

std::uint64_t ComputeSamplingStride(std::uint64_t vertexCount, std::uint64_t maxRenderPoints) {
  if (maxRenderPoints == 0 || vertexCount <= maxRenderPoints) {
    return 1;
  }

  return (vertexCount + maxRenderPoints - 1) / maxRenderPoints;
}

void FlushChunk(const PlyPointChunkCallback& onChunk, PointCloudChunk& chunk) {
  if (!onChunk || chunk.points.empty()) {
    return;
  }

  onChunk(std::move(chunk));
  chunk = {};
}

}  // namespace

PointCloudData LoadAsciiPly(
  const std::filesystem::path& path,
  const PlyLoadOptions& options,
  std::stop_token stopToken,
  const PlyLoadProgressCallback& onProgress,
  const PlyPointChunkCallback& onChunk) {
  std::vector<char> readBuffer(1 << 20);
  std::filebuf fileBuffer;
  fileBuffer.pubsetbuf(readBuffer.data(), static_cast<std::streamsize>(readBuffer.size()));
  if (fileBuffer.open(path, std::ios::in) == nullptr) {
    throw std::runtime_error("Failed to open file.");
  }
  std::istream input(&fileBuffer);

  const std::uint64_t totalBytes = static_cast<std::uint64_t>(std::filesystem::file_size(path));
  ReportProgress(onProgress, 0, totalBytes, 0, 0, "Reading header");

  HeaderInfo header = ParseHeader(input);

  const std::optional<std::size_t> xIndex = FindPropertyIndex(header.vertexProperties, "x");
  const std::optional<std::size_t> yIndex = FindPropertyIndex(header.vertexProperties, "y");
  const std::optional<std::size_t> zIndex = FindPropertyIndex(header.vertexProperties, "z");
  if (!xIndex || !yIndex || !zIndex) {
    throw std::runtime_error("PLY vertex element must contain x, y and z properties.");
  }

  const std::optional<std::size_t> redIndex = FindPropertyIndex(header.vertexProperties, "red");
  const std::optional<std::size_t> greenIndex = FindPropertyIndex(header.vertexProperties, "green");
  const std::optional<std::size_t> blueIndex = FindPropertyIndex(header.vertexProperties, "blue");
  const std::optional<std::size_t> alphaIndex = FindPropertyIndex(header.vertexProperties, "alpha");
  const std::size_t lastRelevantPropertyIndex = std::max({
    *xIndex,
    *yIndex,
    *zIndex,
    redIndex.value_or(0),
    greenIndex.value_or(0),
    blueIndex.value_or(0),
    alphaIndex.value_or(0),
  });
  const std::uint64_t samplingStride = ComputeSamplingStride(header.vertexCount, options.maxRenderPoints);

  PointCloudData result;
  result.sourcePath = path;
  result.sourcePointCount = header.vertexCount;
  result.sampledRender = samplingStride > 1;
  result.samplingStride = samplingStride;

  PointCloudChunk chunk;
  chunk.points.reserve(std::max<std::size_t>(options.streamBatchPoints, 1));

  std::string line;
  line.reserve(256);
  std::uint64_t bytesRead = header.bytesConsumed;
  const std::string progressStatus = result.sampledRender ? "Streaming sampled points" : "Streaming points";

  for (std::uint64_t pointIndex = 0; pointIndex < header.vertexCount; ++pointIndex) {
    if (stopToken.stop_requested()) {
      throw std::runtime_error("Loading cancelled.");
    }

    if (!std::getline(input, line)) {
      throw std::runtime_error("PLY file ended before all vertices were read.");
    }

    bytesRead += static_cast<std::uint64_t>(line.size() + 1);
    PointVertex vertex = ParseVertex(
      line,
      lastRelevantPropertyIndex,
      *xIndex,
      *yIndex,
      *zIndex,
      redIndex,
      greenIndex,
      blueIndex,
      alphaIndex);

    result.bounds.Expand({vertex.x, vertex.y, vertex.z});

    if ((pointIndex % samplingStride) == 0) {
      chunk.points.push_back(vertex);
      ++result.renderPointCount;

      if (chunk.points.size() >= std::max<std::size_t>(options.streamBatchPoints, 1)) {
        chunk.bounds = result.bounds;
        chunk.sourcePointsRead = pointIndex + 1;
        chunk.sourcePointCount = header.vertexCount;
        chunk.renderedPointCount = result.renderPointCount;
        chunk.sampledRender = result.sampledRender;
        chunk.samplingStride = result.samplingStride;
        FlushChunk(onChunk, chunk);
      }
    }

    if ((pointIndex + 1) % 50'000 == 0 || pointIndex + 1 == header.vertexCount) {
      ReportProgress(
        onProgress,
        bytesRead,
        totalBytes,
        pointIndex + 1,
        result.renderPointCount,
        progressStatus);
    }
  }

  chunk.bounds = result.bounds;
  chunk.sourcePointsRead = result.sourcePointCount;
  chunk.sourcePointCount = result.sourcePointCount;
  chunk.renderedPointCount = result.renderPointCount;
  chunk.sampledRender = result.sampledRender;
  chunk.samplingStride = result.samplingStride;
  FlushChunk(onChunk, chunk);

  ReportProgress(
    onProgress,
    totalBytes,
    totalBytes,
    result.sourcePointCount,
    result.renderPointCount,
    "Ready");

  return result;
}

}  // namespace pointmod
