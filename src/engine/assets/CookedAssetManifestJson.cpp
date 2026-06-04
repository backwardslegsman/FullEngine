#include "engine/assets/CookedAssetManifestJson.hpp"

#include <cerrno>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <limits>
#include <sstream>
#include <string>

namespace full_engine
{
namespace
{
const char* recordAssetName = "asset";
const char* recordTerrainChunkName = "terrainChunk";

bool parseAssetKind(const std::string& value, AssetKind& kind) noexcept
{
    if (value == "Unknown")
    {
        kind = AssetKind::Unknown;
        return true;
    }
    if (value == "Mesh")
    {
        kind = AssetKind::Mesh;
        return true;
    }
    if (value == "Material")
    {
        kind = AssetKind::Material;
        return true;
    }
    if (value == "Texture")
    {
        kind = AssetKind::Texture;
        return true;
    }
    if (value == "TerrainChunk")
    {
        kind = AssetKind::TerrainChunk;
        return true;
    }
    if (value == "Skeleton")
    {
        kind = AssetKind::Skeleton;
        return true;
    }
    if (value == "SkinnedMesh")
    {
        kind = AssetKind::SkinnedMesh;
        return true;
    }
    if (value == "AnimationClip")
    {
        kind = AssetKind::AnimationClip;
        return true;
    }
    if (value == "Shader")
    {
        kind = AssetKind::Shader;
        return true;
    }

    return false;
}

bool parseStringField(const std::string& line, const char* name, std::string& value)
{
    const std::string prefix = std::string("\"") + name + "\":\"";
    const std::size_t start = line.find(prefix);
    if (start == std::string::npos)
    {
        return false;
    }

    const std::size_t valueStart = start + prefix.size();
    const std::size_t valueEnd = line.find('"', valueStart);
    if (valueEnd == std::string::npos)
    {
        return false;
    }

    value = line.substr(valueStart, valueEnd - valueStart);
    return true;
}

bool parseUnsignedField(const std::string& line, const char* name, std::uint64_t& value)
{
    const std::string prefix = std::string("\"") + name + "\":";
    const std::size_t start = line.find(prefix);
    if (start == std::string::npos)
    {
        return false;
    }

    std::size_t cursor = start + prefix.size();
    if (cursor >= line.size() || !std::isdigit(static_cast<unsigned char>(line[cursor])))
    {
        return false;
    }

    std::uint64_t parsed = 0;
    while (cursor < line.size() && std::isdigit(static_cast<unsigned char>(line[cursor])))
    {
        const std::uint64_t digit = static_cast<std::uint64_t>(line[cursor] - '0');
        if (parsed > (std::numeric_limits<std::uint64_t>::max() - digit) / 10U)
        {
            return false;
        }
        parsed = parsed * 10U + digit;
        ++cursor;
    }

    if (cursor >= line.size() || (line[cursor] != ',' && line[cursor] != '}'))
    {
        return false;
    }

    value = parsed;
    return true;
}

bool parseUnsigned32Field(const std::string& line, const char* name, std::uint32_t& value)
{
    std::uint64_t parsed = 0;
    if (!parseUnsignedField(line, name, parsed) || parsed > std::numeric_limits<std::uint32_t>::max())
    {
        return false;
    }

    value = static_cast<std::uint32_t>(parsed);
    return true;
}

bool parseSigned32Field(const std::string& line, const char* name, std::int32_t& value)
{
    const std::string prefix = std::string("\"") + name + "\":";
    const std::size_t start = line.find(prefix);
    if (start == std::string::npos)
    {
        return false;
    }

    std::size_t cursor = start + prefix.size();
    if (cursor >= line.size())
    {
        return false;
    }

    bool negative = false;
    if (line[cursor] == '-')
    {
        negative = true;
        ++cursor;
    }

    if (cursor >= line.size() || !std::isdigit(static_cast<unsigned char>(line[cursor])))
    {
        return false;
    }

    std::int64_t parsed = 0;
    const std::int64_t limit = negative
        ? -static_cast<std::int64_t>(std::numeric_limits<std::int32_t>::min())
        : static_cast<std::int64_t>(std::numeric_limits<std::int32_t>::max());
    while (cursor < line.size() && std::isdigit(static_cast<unsigned char>(line[cursor])))
    {
        const std::int64_t digit = static_cast<std::int64_t>(line[cursor] - '0');
        if (parsed > (limit - digit) / 10)
        {
            return false;
        }
        parsed = parsed * 10 + digit;
        ++cursor;
    }

    if (cursor >= line.size() || (line[cursor] != ',' && line[cursor] != '}'))
    {
        return false;
    }

    value = static_cast<std::int32_t>(negative ? -parsed : parsed);
    return true;
}

bool parseFloatField(const std::string& line, const char* name, float& value)
{
    const std::string prefix = std::string("\"") + name + "\":";
    const std::size_t start = line.find(prefix);
    if (start == std::string::npos)
    {
        return false;
    }

    const char* begin = line.c_str() + start + prefix.size();
    char* end = nullptr;
    errno = 0;
    const float parsed = std::strtof(begin, &end);
    if (begin == end || errno == ERANGE || !std::isfinite(parsed))
    {
        return false;
    }

    if (*end != ',' && *end != '}')
    {
        return false;
    }

    value = parsed;
    return true;
}

std::string indexedField(const char* prefix, const std::uint32_t index, const char* suffix)
{
    std::ostringstream stream;
    stream << prefix << index << suffix;
    return stream.str();
}

void writeAsset(std::ostream& stream, const AssetRecord& record)
{
    stream << "{\"record\":\"" << recordAssetName << "\"";
    stream << ",\"id\":" << record.id.value;
    stream << ",\"kind\":\"" << assetKindName(record.kind) << "\"";
    stream << ",\"dependencyCount\":" << record.dependencyCount;
    const std::uint32_t dependencyCount = record.dependencyCount <= kMaxAssetDependencies
        ? record.dependencyCount
        : kMaxAssetDependencies;
    for (std::uint32_t index = 0; index < dependencyCount; ++index)
    {
        stream << ",\"dependency" << index << "Id\":" << record.dependencies[index].id.value;
        stream << ",\"dependency" << index << "Kind\":\"" << assetKindName(record.dependencies[index].kind) << "\"";
    }
    stream << "}\n";
}

void writeTerrain(std::ostream& stream, const TerrainChunkAssetDesc& desc)
{
    stream << "{\"record\":\"" << recordTerrainChunkName << "\"";
    stream << ",\"chunkX\":" << desc.id.x;
    stream << ",\"chunkY\":" << desc.id.y;
    stream << ",\"chunkZ\":" << desc.id.z;
    stream << ",\"lodCount\":" << desc.lodCount;
    const std::uint32_t lodCount = desc.lodCount <= kMaxTerrainAssetLodLevels
        ? desc.lodCount
        : kMaxTerrainAssetLodLevels;
    stream << std::setprecision(std::numeric_limits<float>::max_digits10);
    for (std::uint32_t index = 0; index < lodCount; ++index)
    {
        stream << ",\"lod" << index << "Mesh\":" << desc.lods[index].mesh.value;
        stream << ",\"lod" << index << "Material\":" << desc.lods[index].material.value;
        stream << ",\"lod" << index << "MaxDistanceMeters\":" << desc.lods[index].maxDistanceMeters;
    }
    stream << ",\"splatMap\":" << desc.splatMap.value;
    stream << "}\n";
}

bool parseAsset(const std::string& line, AssetRecord& record)
{
    std::string kindName;
    if (!parseUnsignedField(line, "id", record.id.value) ||
        !parseStringField(line, "kind", kindName) ||
        !parseAssetKind(kindName, record.kind) ||
        !parseUnsigned32Field(line, "dependencyCount", record.dependencyCount))
    {
        return false;
    }

    if (record.dependencyCount > kMaxAssetDependencies)
    {
        return true;
    }

    for (std::uint32_t index = 0; index < record.dependencyCount; ++index)
    {
        const std::string idField = indexedField("dependency", index, "Id");
        const std::string kindField = indexedField("dependency", index, "Kind");
        std::string dependencyKindName;
        if (!parseUnsignedField(line, idField.c_str(), record.dependencies[index].id.value) ||
            !parseStringField(line, kindField.c_str(), dependencyKindName) ||
            !parseAssetKind(dependencyKindName, record.dependencies[index].kind))
        {
            return false;
        }
    }

    return true;
}

bool parseTerrain(const std::string& line, TerrainChunkAssetDesc& desc)
{
    if (!parseSigned32Field(line, "chunkX", desc.id.x) ||
        !parseSigned32Field(line, "chunkY", desc.id.y) ||
        !parseSigned32Field(line, "chunkZ", desc.id.z) ||
        !parseUnsigned32Field(line, "lodCount", desc.lodCount))
    {
        return false;
    }

    if (desc.lodCount <= kMaxTerrainAssetLodLevels)
    {
        for (std::uint32_t index = 0; index < desc.lodCount; ++index)
        {
            const std::string meshField = indexedField("lod", index, "Mesh");
            const std::string materialField = indexedField("lod", index, "Material");
            const std::string distanceField = indexedField("lod", index, "MaxDistanceMeters");
            if (!parseUnsignedField(line, meshField.c_str(), desc.lods[index].mesh.value) ||
                !parseUnsignedField(line, materialField.c_str(), desc.lods[index].material.value) ||
                !parseFloatField(line, distanceField.c_str(), desc.lods[index].maxDistanceMeters))
            {
                return false;
            }
        }
    }

    return parseUnsignedField(line, "splatMap", desc.splatMap.value);
}

bool parseLine(const std::string& line, CookedAssetManifest& manifest)
{
    if (line.size() < 2 || line.front() != '{' || line.back() != '}')
    {
        return false;
    }

    std::string recordType;
    if (!parseStringField(line, "record", recordType))
    {
        return false;
    }

    if (recordType == recordAssetName)
    {
        AssetRecord record;
        if (!parseAsset(line, record))
        {
            return false;
        }
        manifest.assets.push_back(record);
        return true;
    }

    if (recordType == recordTerrainChunkName)
    {
        TerrainChunkAssetDesc desc;
        if (!parseTerrain(line, desc))
        {
            return false;
        }
        manifest.terrainChunks.push_back(desc);
        return true;
    }

    return false;
}
} // namespace

const char* cookedAssetManifestExportResultName(const CookedAssetManifestExportResult result) noexcept
{
    switch (result)
    {
    case CookedAssetManifestExportResult::Success:
        return "Success";
    case CookedAssetManifestExportResult::InvalidArgument:
        return "InvalidArgument";
    case CookedAssetManifestExportResult::IoError:
        return "IoError";
    }

    return "Unknown";
}

const char* cookedAssetManifestImportResultName(const CookedAssetManifestImportResult result) noexcept
{
    switch (result)
    {
    case CookedAssetManifestImportResult::Success:
        return "Success";
    case CookedAssetManifestImportResult::InvalidArgument:
        return "InvalidArgument";
    case CookedAssetManifestImportResult::IoError:
        return "IoError";
    case CookedAssetManifestImportResult::ParseError:
        return "ParseError";
    case CookedAssetManifestImportResult::ValidationError:
        return "ValidationError";
    }

    return "Unknown";
}

const char* assetKindName(const AssetKind kind) noexcept
{
    switch (kind)
    {
    case AssetKind::Unknown:
        return "Unknown";
    case AssetKind::Mesh:
        return "Mesh";
    case AssetKind::Material:
        return "Material";
    case AssetKind::Texture:
        return "Texture";
    case AssetKind::TerrainChunk:
        return "TerrainChunk";
    case AssetKind::Skeleton:
        return "Skeleton";
    case AssetKind::SkinnedMesh:
        return "SkinnedMesh";
    case AssetKind::AnimationClip:
        return "AnimationClip";
    case AssetKind::Shader:
        return "Shader";
    }

    return "Unknown";
}

CookedAssetManifestExportResult exportCookedAssetManifestJsonLines(
    const CookedAssetManifest& manifest,
    const char* path)
{
    if (path == nullptr || path[0] == '\0')
    {
        return CookedAssetManifestExportResult::InvalidArgument;
    }

    std::ofstream output(path, std::ios::out | std::ios::trunc);
    if (!output.is_open())
    {
        return CookedAssetManifestExportResult::IoError;
    }

    for (const AssetRecord& record : manifest.assets)
    {
        writeAsset(output, record);
    }
    for (const TerrainChunkAssetDesc& desc : manifest.terrainChunks)
    {
        writeTerrain(output, desc);
    }

    return output.good() ? CookedAssetManifestExportResult::Success
                         : CookedAssetManifestExportResult::IoError;
}

CookedAssetManifestImport importCookedAssetManifestJsonLines(const char* path)
{
    CookedAssetManifestImport imported;
    if (path == nullptr || path[0] == '\0')
    {
        imported.result = CookedAssetManifestImportResult::InvalidArgument;
        return imported;
    }

    std::ifstream input(path);
    if (!input.is_open())
    {
        imported.result = CookedAssetManifestImportResult::IoError;
        return imported;
    }

    CookedAssetManifest parsed;
    std::string line;
    while (std::getline(input, line))
    {
        if (!parseLine(line, parsed))
        {
            imported.manifest = {};
            imported.result = CookedAssetManifestImportResult::ParseError;
            return imported;
        }
    }

    if (input.bad())
    {
        imported.result = CookedAssetManifestImportResult::IoError;
        return imported;
    }

    imported.validation = validateCookedAssetManifest(parsed);
    if (imported.validation.result != CookedAssetManifestValidationResult::Success)
    {
        imported.manifest = {};
        imported.result = CookedAssetManifestImportResult::ValidationError;
        return imported;
    }

    imported.manifest = parsed;
    imported.result = CookedAssetManifestImportResult::Success;
    return imported;
}
} // namespace full_engine
