#include "PlyAsciiLoader.hpp"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <optional>
#include <random>
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

HeaderInfo ParseHeader(std::ifstream& input) {
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
  std::size_t propertyCount,
  std::size_t xIndex,
  std::size_t yIndex,
  std::size_t zIndex,
  std::optional<std::size_t> redIndex,
  std::optional<std::size_t> greenIndex,
  std::optional<std::size_t> blueIndex,
  std::optional<std::size_t> alphaIndex) {
  PointVertex vertex{};
  char* cursor = line.data();

  for (std::size_t propertyIndex = 0; propertyIndex < propertyCount; ++propertyIndex) {
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

}  // namespace

PointCloudData LoadAsciiPlyPreview(
  const std::filesystem::path& path,
  const PlyLoadOptions& options,
  std::stop_token stopToken,
  const PlyLoadProgressCallback& onProgress) {
  std::ifstream input(path);
  if (!input.is_open()) {
    throw std::runtime_error("Failed to open file.");
  }

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

  PointCloudData result;
  result.sourcePath = path;
  result.sourcePointCount = header.vertexCount;
  result.points.reserve(std::min<std::size_t>(options.maxPreviewPoints, static_cast<std::size_t>(header.vertexCount)));

  std::mt19937_64 random{0x706f696e746d6f64ULL};
  std::string line;
  std::uint64_t bytesRead = header.bytesConsumed;

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
      header.vertexProperties.size(),
      *xIndex,
      *yIndex,
      *zIndex,
      redIndex,
      greenIndex,
      blueIndex,
      alphaIndex);

    result.bounds.Expand({vertex.x, vertex.y, vertex.z});

    if (result.points.size() < options.maxPreviewPoints) {
      result.points.push_back(vertex);
    } else {
      std::uniform_int_distribution<std::uint64_t> distribution(0, pointIndex);
      const std::uint64_t replacementIndex = distribution(random);
      if (replacementIndex < result.points.size()) {
        result.points[static_cast<std::size_t>(replacementIndex)] = vertex;
      }
    }

    if ((pointIndex + 1) % 50'000 == 0 || pointIndex + 1 == header.vertexCount) {
      ReportProgress(
        onProgress,
        bytesRead,
        totalBytes,
        pointIndex + 1,
        static_cast<std::uint64_t>(result.points.size()),
        "Sampling preview");
    }
  }

  result.previewPointCount = static_cast<std::uint64_t>(result.points.size());
  result.sampledPreview = result.previewPointCount < result.sourcePointCount;
  ReportProgress(
    onProgress,
    totalBytes,
    totalBytes,
    result.sourcePointCount,
    result.previewPointCount,
    "Ready");

  return result;
}

}  // namespace pointmod
