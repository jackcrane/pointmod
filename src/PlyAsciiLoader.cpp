#include "PlyAsciiLoader.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <ios>
#include <limits>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace pointmod {

namespace {

enum class PlyFormat {
  kAscii,
  kBinaryLittleEndian,
  kBinaryBigEndian,
};

enum class PlyScalarType {
  kInt8,
  kUInt8,
  kInt16,
  kUInt16,
  kInt32,
  kUInt32,
  kFloat32,
  kFloat64,
};

struct PlyPropertyInfo {
  std::string name;
  bool isList = false;
  PlyScalarType scalarType = PlyScalarType::kFloat32;
  PlyScalarType listCountType = PlyScalarType::kUInt8;
  PlyScalarType listValueType = PlyScalarType::kUInt8;
};

struct PlyElementInfo {
  std::string name;
  std::uint64_t count = 0;
  std::vector<PlyPropertyInfo> properties;
};

struct HeaderInfo {
  PlyFormat format = PlyFormat::kAscii;
  std::vector<PlyElementInfo> elements;
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

PlyScalarType ParseScalarType(std::string_view value) {
  if (value == "char" || value == "int8") {
    return PlyScalarType::kInt8;
  }
  if (value == "uchar" || value == "uint8") {
    return PlyScalarType::kUInt8;
  }
  if (value == "short" || value == "int16") {
    return PlyScalarType::kInt16;
  }
  if (value == "ushort" || value == "uint16") {
    return PlyScalarType::kUInt16;
  }
  if (value == "int" || value == "int32") {
    return PlyScalarType::kInt32;
  }
  if (value == "uint" || value == "uint32") {
    return PlyScalarType::kUInt32;
  }
  if (value == "float" || value == "float32") {
    return PlyScalarType::kFloat32;
  }
  if (value == "double" || value == "float64") {
    return PlyScalarType::kFloat64;
  }

  throw std::runtime_error("PLY file uses an unsupported scalar type.");
}

bool IsIntegerType(PlyScalarType type) {
  switch (type) {
    case PlyScalarType::kInt8:
    case PlyScalarType::kUInt8:
    case PlyScalarType::kInt16:
    case PlyScalarType::kUInt16:
    case PlyScalarType::kInt32:
    case PlyScalarType::kUInt32:
      return true;
    case PlyScalarType::kFloat32:
    case PlyScalarType::kFloat64:
      return false;
  }

  return false;
}

bool IsSignedIntegerType(PlyScalarType type) {
  switch (type) {
    case PlyScalarType::kInt8:
    case PlyScalarType::kInt16:
    case PlyScalarType::kInt32:
      return true;
    case PlyScalarType::kUInt8:
    case PlyScalarType::kUInt16:
    case PlyScalarType::kUInt32:
    case PlyScalarType::kFloat32:
    case PlyScalarType::kFloat64:
      return false;
  }

  return false;
}

std::size_t ScalarTypeSize(PlyScalarType type) {
  switch (type) {
    case PlyScalarType::kInt8:
    case PlyScalarType::kUInt8:
      return 1;
    case PlyScalarType::kInt16:
    case PlyScalarType::kUInt16:
      return 2;
    case PlyScalarType::kInt32:
    case PlyScalarType::kUInt32:
    case PlyScalarType::kFloat32:
      return 4;
    case PlyScalarType::kFloat64:
      return 8;
  }

  return 0;
}

bool IsLittleEndian(PlyFormat format) {
  return format == PlyFormat::kBinaryLittleEndian;
}

HeaderInfo ParseHeader(std::istream& input) {
  HeaderInfo info;
  std::string line;
  bool sawFormat = false;
  bool headerStarted = false;
  PlyElementInfo* currentElement = nullptr;

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
      std::istringstream stream(trimmed);
      std::string keyword;
      std::string formatValue;
      stream >> keyword >> formatValue;
      if (formatValue == "ascii") {
        info.format = PlyFormat::kAscii;
      } else if (formatValue == "binary_little_endian") {
        info.format = PlyFormat::kBinaryLittleEndian;
      } else if (formatValue == "binary_big_endian") {
        info.format = PlyFormat::kBinaryBigEndian;
      } else {
        throw std::runtime_error("PLY file uses an unsupported format.");
      }
      continue;
    }

    if (StartsWith(trimmed, "element ")) {
      std::istringstream stream(trimmed);
      std::string elementKeyword;
      std::string elementName;
      std::uint64_t elementCount = 0;
      stream >> elementKeyword >> elementName;
      stream >> elementCount;
      info.elements.push_back(PlyElementInfo{
        .name = std::move(elementName),
        .count = elementCount,
      });
      currentElement = &info.elements.back();
      continue;
    }

    if (StartsWith(trimmed, "property ")) {
      if (currentElement == nullptr) {
        throw std::runtime_error("PLY header declared a property before any element.");
      }

      std::istringstream stream(trimmed);
      std::string propertyKeyword;
      std::string propertyType;
      stream >> propertyKeyword >> propertyType;
      if (propertyType == "list") {
        std::string countType;
        std::string valueType;
        std::string propertyName;
        stream >> countType >> valueType >> propertyName;
        currentElement->properties.push_back(PlyPropertyInfo{
          .name = std::move(propertyName),
          .isList = true,
          .scalarType = ParseScalarType(valueType),
          .listCountType = ParseScalarType(countType),
          .listValueType = ParseScalarType(valueType),
        });
      } else {
        std::string propertyName;
        stream >> propertyName;
        currentElement->properties.push_back(PlyPropertyInfo{
          .name = std::move(propertyName),
          .isList = false,
          .scalarType = ParseScalarType(propertyType),
        });
      }
      continue;
    }

    if (trimmed == "end_header") {
      if (!sawFormat) {
        throw std::runtime_error("PLY header is missing a format declaration.");
      }
      return info;
    }
  }

  throw std::runtime_error("PLY header ended unexpectedly.");
}

std::optional<std::size_t> FindPropertyIndex(
  const PlyElementInfo& element,
  std::string_view propertyName) {
  for (std::size_t index = 0; index < element.properties.size(); ++index) {
    const PlyPropertyInfo& property = element.properties[index];
    if (!property.isList && property.name == propertyName) {
      return index;
    }
  }
  return std::nullopt;
}

const PlyElementInfo& FindVertexElement(const HeaderInfo& header) {
  for (const PlyElementInfo& element : header.elements) {
    if (element.name == "vertex") {
      return element;
    }
  }

  throw std::runtime_error("PLY file is missing a vertex element.");
}

double ParseNextAsciiScalar(char*& cursor) {
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

std::uint64_t ParseAsciiListCount(char*& cursor) {
  const double value = ParseNextAsciiScalar(cursor);
  if (value < 0.0) {
    throw std::runtime_error("PLY list property used a negative item count.");
  }

  return static_cast<std::uint64_t>(value);
}

std::uint64_t CheckedMultiply(std::uint64_t left, std::uint64_t right) {
  if (left == 0 || right == 0) {
    return 0;
  }
  if (left > (std::numeric_limits<std::uint64_t>::max)() / right) {
    throw std::runtime_error("PLY file contains oversized list data.");
  }

  return left * right;
}

void SkipExact(std::istream& input, std::uint64_t byteCount) {
  while (byteCount > 0) {
    const std::streamsize chunk = static_cast<std::streamsize>((std::min)(
      byteCount,
      static_cast<std::uint64_t>((std::numeric_limits<std::streamsize>::max)())));
    input.ignore(chunk);
    if (input.gcount() != chunk) {
      throw std::runtime_error("PLY file ended before all binary data was read.");
    }
    byteCount -= static_cast<std::uint64_t>(chunk);
  }
}

std::uint64_t ReadBinaryUnsignedInteger(
  std::istream& input,
  PlyScalarType type,
  PlyFormat format) {
  if (!IsIntegerType(type)) {
    throw std::runtime_error("PLY file uses a non-integer list count type.");
  }

  const std::size_t byteCount = ScalarTypeSize(type);
  std::array<unsigned char, 8> bytes{};
  input.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(byteCount));
  if (input.gcount() != static_cast<std::streamsize>(byteCount)) {
    throw std::runtime_error("PLY file ended before all binary data was read.");
  }

  std::uint64_t value = 0;
  if (IsLittleEndian(format)) {
    for (std::size_t index = 0; index < byteCount; ++index) {
      value |= static_cast<std::uint64_t>(bytes[index]) << (index * 8);
    }
  } else {
    for (std::size_t index = 0; index < byteCount; ++index) {
      value = (value << 8) | static_cast<std::uint64_t>(bytes[index]);
    }
  }

  if (IsSignedIntegerType(type)) {
    const std::size_t bitCount = byteCount * 8;
    const std::uint64_t signMask = std::uint64_t{1} << (bitCount - 1);
    if ((value & signMask) != 0) {
      throw std::runtime_error("PLY list property used a negative item count.");
    }
  }

  return value;
}

double ReadBinaryScalar(std::istream& input, PlyScalarType type, PlyFormat format) {
  const std::size_t byteCount = ScalarTypeSize(type);
  std::array<unsigned char, 8> bytes{};
  input.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(byteCount));
  if (input.gcount() != static_cast<std::streamsize>(byteCount)) {
    throw std::runtime_error("PLY file ended before all binary data was read.");
  }

  std::uint64_t bits = 0;
  if (IsLittleEndian(format)) {
    for (std::size_t index = 0; index < byteCount; ++index) {
      bits |= static_cast<std::uint64_t>(bytes[index]) << (index * 8);
    }
  } else {
    for (std::size_t index = 0; index < byteCount; ++index) {
      bits = (bits << 8) | static_cast<std::uint64_t>(bytes[index]);
    }
  }

  switch (type) {
    case PlyScalarType::kInt8:
      return static_cast<double>(static_cast<std::int8_t>(bits));
    case PlyScalarType::kUInt8:
      return static_cast<double>(static_cast<std::uint8_t>(bits));
    case PlyScalarType::kInt16:
      return static_cast<double>(static_cast<std::int16_t>(bits));
    case PlyScalarType::kUInt16:
      return static_cast<double>(static_cast<std::uint16_t>(bits));
    case PlyScalarType::kInt32:
      return static_cast<double>(static_cast<std::int32_t>(bits));
    case PlyScalarType::kUInt32:
      return static_cast<double>(static_cast<std::uint32_t>(bits));
    case PlyScalarType::kFloat32: {
      const std::uint32_t floatBits = static_cast<std::uint32_t>(bits);
      float value = 0.0f;
      std::memcpy(&value, &floatBits, sizeof(value));
      return static_cast<double>(value);
    }
    case PlyScalarType::kFloat64: {
      double value = 0.0;
      std::memcpy(&value, &bits, sizeof(value));
      return value;
    }
  }

  return 0.0;
}

PointVertex ParseAsciiVertex(
  std::string& line,
  const PlyElementInfo& vertexElement,
  std::size_t xIndex,
  std::size_t yIndex,
  std::size_t zIndex,
  std::optional<std::size_t> redIndex,
  std::optional<std::size_t> greenIndex,
  std::optional<std::size_t> blueIndex,
  std::optional<std::size_t> alphaIndex) {
  PointVertex vertex{};
  char* cursor = line.data();

  for (std::size_t propertyIndex = 0; propertyIndex < vertexElement.properties.size(); ++propertyIndex) {
    const PlyPropertyInfo& property = vertexElement.properties[propertyIndex];
    if (property.isList) {
      const std::uint64_t itemCount = ParseAsciiListCount(cursor);
      for (std::uint64_t itemIndex = 0; itemIndex < itemCount; ++itemIndex) {
        ParseNextAsciiScalar(cursor);
      }
      continue;
    }

    const double value = ParseNextAsciiScalar(cursor);

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

PointVertex ParseBinaryVertex(
  std::istream& input,
  const PlyElementInfo& vertexElement,
  PlyFormat format,
  std::uint64_t& bytesRead,
  std::size_t xIndex,
  std::size_t yIndex,
  std::size_t zIndex,
  std::optional<std::size_t> redIndex,
  std::optional<std::size_t> greenIndex,
  std::optional<std::size_t> blueIndex,
  std::optional<std::size_t> alphaIndex) {
  PointVertex vertex{};

  for (std::size_t propertyIndex = 0; propertyIndex < vertexElement.properties.size(); ++propertyIndex) {
    const PlyPropertyInfo& property = vertexElement.properties[propertyIndex];
    if (property.isList) {
      const std::uint64_t itemCount = ReadBinaryUnsignedInteger(input, property.listCountType, format);
      const std::uint64_t payloadBytes = CheckedMultiply(itemCount, ScalarTypeSize(property.listValueType));
      bytesRead += static_cast<std::uint64_t>(ScalarTypeSize(property.listCountType)) + payloadBytes;
      SkipExact(input, payloadBytes);
      continue;
    }

    bytesRead += static_cast<std::uint64_t>(ScalarTypeSize(property.scalarType));
    const double value = ReadBinaryScalar(input, property.scalarType, format);
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

std::uint64_t SkipAsciiElement(
  std::istream& input,
  const PlyElementInfo& element,
  std::stop_token stopToken) {
  (void)element;

  std::uint64_t bytesRead = 0;
  std::string line;
  for (std::uint64_t entryIndex = 0; entryIndex < element.count; ++entryIndex) {
    if (stopToken.stop_requested()) {
      throw std::runtime_error("Loading cancelled.");
    }
    if (!std::getline(input, line)) {
      throw std::runtime_error("PLY file ended before all elements were read.");
    }
    bytesRead += static_cast<std::uint64_t>(line.size() + 1);
  }

  return bytesRead;
}

std::uint64_t SkipBinaryElement(
  std::istream& input,
  const PlyElementInfo& element,
  PlyFormat format,
  std::stop_token stopToken) {
  std::uint64_t bytesRead = 0;
  for (std::uint64_t entryIndex = 0; entryIndex < element.count; ++entryIndex) {
    if (stopToken.stop_requested()) {
      throw std::runtime_error("Loading cancelled.");
    }

    for (const PlyPropertyInfo& property : element.properties) {
      if (property.isList) {
        const std::uint64_t itemCount = ReadBinaryUnsignedInteger(input, property.listCountType, format);
        const std::uint64_t payloadBytes = CheckedMultiply(itemCount, ScalarTypeSize(property.listValueType));
        bytesRead += static_cast<std::uint64_t>(ScalarTypeSize(property.listCountType)) + payloadBytes;
        SkipExact(input, payloadBytes);
        continue;
      }

      const std::size_t scalarBytes = ScalarTypeSize(property.scalarType);
      bytesRead += static_cast<std::uint64_t>(scalarBytes);
      SkipExact(input, scalarBytes);
    }
  }

  return bytesRead;
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

void ReportSaveProgress(
  const PlySaveProgressCallback& onProgress,
  std::uint64_t pointsWritten,
  std::uint64_t totalPoints,
  std::string status) {
  if (!onProgress) {
    return;
  }

  onProgress(PlySaveProgress{
    .pointsWritten = pointsWritten,
    .totalPoints = totalPoints,
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

PointCloudData LoadPly(
  const std::filesystem::path& path,
  const PlyLoadOptions& options,
  std::stop_token stopToken,
  const PlyLoadProgressCallback& onProgress,
  const PlyPointChunkCallback& onChunk) {
  std::vector<char> readBuffer(1 << 20);
  std::filebuf fileBuffer;
  fileBuffer.pubsetbuf(readBuffer.data(), static_cast<std::streamsize>(readBuffer.size()));
  if (fileBuffer.open(path, std::ios::in | std::ios::binary) == nullptr) {
    throw std::runtime_error("Failed to open file.");
  }
  std::istream input(&fileBuffer);

  const std::uint64_t totalBytes = static_cast<std::uint64_t>(std::filesystem::file_size(path));
  ReportProgress(onProgress, 0, totalBytes, 0, 0, "Reading header");

  HeaderInfo header = ParseHeader(input);
  const PlyElementInfo& vertexElement = FindVertexElement(header);

  const std::optional<std::size_t> xIndex = FindPropertyIndex(vertexElement, "x");
  const std::optional<std::size_t> yIndex = FindPropertyIndex(vertexElement, "y");
  const std::optional<std::size_t> zIndex = FindPropertyIndex(vertexElement, "z");
  if (!xIndex || !yIndex || !zIndex) {
    throw std::runtime_error("PLY vertex element must contain x, y and z properties.");
  }

  const std::optional<std::size_t> redIndex = FindPropertyIndex(vertexElement, "red");
  const std::optional<std::size_t> greenIndex = FindPropertyIndex(vertexElement, "green");
  const std::optional<std::size_t> blueIndex = FindPropertyIndex(vertexElement, "blue");
  const std::optional<std::size_t> alphaIndex = FindPropertyIndex(vertexElement, "alpha");
  const std::uint64_t samplingStride = ComputeSamplingStride(vertexElement.count, options.maxRenderPoints);

  PointCloudData result;
  result.sourcePath = path;
  result.sourcePointCount = vertexElement.count;
  result.sampledRender = samplingStride > 1;
  result.samplingStride = samplingStride;

  PointCloudChunk chunk;
  chunk.points.reserve(std::max<std::size_t>(options.streamBatchPoints, 1));

  std::uint64_t bytesRead = header.bytesConsumed;
  const std::string progressStatus = result.sampledRender ? "Streaming sampled points" : "Streaming points";
  bool vertexRead = false;
  std::string line;
  line.reserve(256);

  for (const PlyElementInfo& element : header.elements) {
    if (element.name != "vertex") {
      if (vertexRead) {
        break;
      }

      bytesRead += header.format == PlyFormat::kAscii
        ? SkipAsciiElement(input, element, stopToken)
        : SkipBinaryElement(input, element, header.format, stopToken);
      continue;
    }

    vertexRead = true;

    for (std::uint64_t pointIndex = 0; pointIndex < element.count; ++pointIndex) {
      if (stopToken.stop_requested()) {
        throw std::runtime_error("Loading cancelled.");
      }

      PointVertex vertex{};
      if (header.format == PlyFormat::kAscii) {
        if (!std::getline(input, line)) {
          throw std::runtime_error("PLY file ended before all vertices were read.");
        }
        bytesRead += static_cast<std::uint64_t>(line.size() + 1);
        vertex = ParseAsciiVertex(
          line,
          element,
          *xIndex,
          *yIndex,
          *zIndex,
          redIndex,
          greenIndex,
          blueIndex,
          alphaIndex);
      } else {
        vertex = ParseBinaryVertex(
          input,
          element,
          header.format,
          bytesRead,
          *xIndex,
          *yIndex,
          *zIndex,
          redIndex,
          greenIndex,
          blueIndex,
          alphaIndex);
      }

      result.bounds.Expand({vertex.x, vertex.y, vertex.z});

      if ((pointIndex % samplingStride) == 0) {
        chunk.points.push_back(vertex);
        ++result.renderPointCount;

        if (chunk.points.size() >= std::max<std::size_t>(options.streamBatchPoints, 1)) {
          chunk.bounds = result.bounds;
          chunk.sourcePointsRead = pointIndex + 1;
          chunk.sourcePointCount = element.count;
          chunk.renderedPointCount = result.renderPointCount;
          chunk.sampledRender = result.sampledRender;
          chunk.samplingStride = result.samplingStride;
          FlushChunk(onChunk, chunk);
        }
      }

      if ((pointIndex + 1) % 50'000 == 0 || pointIndex + 1 == element.count) {
        ReportProgress(
          onProgress,
          bytesRead,
          totalBytes,
          pointIndex + 1,
          result.renderPointCount,
          progressStatus);
      }
    }

    break;
  }

  if (!vertexRead) {
    throw std::runtime_error("PLY file is missing a vertex element.");
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

void SaveAsciiPly(
  const std::filesystem::path& path,
  const std::vector<PointVertex>& points,
  const PlySaveProgressCallback& onProgress) {
  std::ofstream output(path, std::ios::out | std::ios::trunc);
  if (!output.is_open()) {
    throw std::runtime_error("Failed to open export path.");
  }

  ReportSaveProgress(onProgress, 0, static_cast<std::uint64_t>(points.size()), "Writing header");
  output << "ply\n";
  output << "format ascii 1.0\n";
  output << "element vertex " << points.size() << '\n';
  output << "property float x\n";
  output << "property float y\n";
  output << "property float z\n";
  output << "property uchar red\n";
  output << "property uchar green\n";
  output << "property uchar blue\n";
  output << "property uchar alpha\n";
  output << "end_header\n";

  output << std::setprecision(std::numeric_limits<float>::max_digits10);
  for (std::size_t pointIndex = 0; pointIndex < points.size(); ++pointIndex) {
    const PointVertex& point = points[pointIndex];
    output
      << point.x << ' '
      << point.y << ' '
      << point.z << ' '
      << static_cast<unsigned int>(point.r) << ' '
      << static_cast<unsigned int>(point.g) << ' '
      << static_cast<unsigned int>(point.b) << ' '
      << static_cast<unsigned int>(point.a) << '\n';

    if ((pointIndex + 1) % 50'000 == 0 || pointIndex + 1 == points.size()) {
      ReportSaveProgress(
        onProgress,
        static_cast<std::uint64_t>(pointIndex + 1),
        static_cast<std::uint64_t>(points.size()),
        "Writing points");
    }
  }

  if (!output.good()) {
    throw std::runtime_error("Failed while writing the exported point cloud.");
  }

  ReportSaveProgress(
    onProgress,
    static_cast<std::uint64_t>(points.size()),
    static_cast<std::uint64_t>(points.size()),
    "Save complete");
}

}  // namespace pointmod
