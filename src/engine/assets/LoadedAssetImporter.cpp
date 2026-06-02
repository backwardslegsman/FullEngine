#include "engine/assets/LoadedAssetImporter.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <limits>
#include <sstream>
#include <string>
#include <vector>

namespace full_engine
{
namespace
{
constexpr std::size_t kMaxTextureBytesForDevImport = 64ULL * 1024ULL * 1024ULL;

struct MeshDevData
{
    bool magicSeen = false;
    bool countsSeen = false;
    bool boundsSeen = false;
    std::uint32_t declaredVertexCount = 0;
    std::uint32_t declaredIndexCount = 0;
    LoadedMeshAsset mesh = {};
};

struct TextureDevData
{
    bool magicSeen = false;
    bool sizeSeen = false;
    bool mipSeen = false;
    bool formatSeen = false;
    bool semanticSeen = false;
    bool colorSpaceSeen = false;
    bool bytesSeen = false;
    std::uint32_t declaredByteCount = 0;
    LoadedTextureAsset texture = {};
};

struct MaterialDevData
{
    bool magicSeen = false;
    bool modelSeen = false;
    bool alphaSeen = false;
    bool texturesSeen = false;
    LoadedMaterialAsset material = {};
};

bool requireEnd(std::istringstream& line)
{
    std::string extra;
    return !(line >> extra);
}

bool readAssetId(std::istringstream& line, AssetId& id)
{
    std::uint64_t value = 0;
    if (!(line >> value))
    {
        return false;
    }
    id = {value};
    return true;
}

bool readFloat(std::istringstream& line, float& value)
{
    return static_cast<bool>(line >> value);
}

bool readUint32(std::istringstream& line, std::uint32_t& value)
{
    return static_cast<bool>(line >> value);
}

bool readUint16(std::istringstream& line, std::uint16_t& value)
{
    std::uint32_t parsed = 0;
    if (!(line >> parsed) || parsed > std::numeric_limits<std::uint16_t>::max())
    {
        return false;
    }
    value = static_cast<std::uint16_t>(parsed);
    return true;
}

bool readUint8(std::istringstream& line, std::uint8_t& value)
{
    std::uint32_t parsed = 0;
    if (!(line >> parsed) || parsed > std::numeric_limits<std::uint8_t>::max())
    {
        return false;
    }
    value = static_cast<std::uint8_t>(parsed);
    return true;
}

bool parseTextureFormat(const std::string& text, AssetSourceTextureFormat& value) noexcept
{
    if (text == "Rgba8")
    {
        value = AssetSourceTextureFormat::Rgba8;
        return true;
    }
    return false;
}

bool parseTextureSemantic(const std::string& text, AssetSourceTextureSemantic& value) noexcept
{
    if (text == "Color")
    {
        value = AssetSourceTextureSemantic::Color;
        return true;
    }
    if (text == "LinearData")
    {
        value = AssetSourceTextureSemantic::LinearData;
        return true;
    }
    if (text == "NormalMap")
    {
        value = AssetSourceTextureSemantic::NormalMap;
        return true;
    }
    if (text == "TerrainSplat")
    {
        value = AssetSourceTextureSemantic::TerrainSplat;
        return true;
    }
    if (text == "ColorGradingLut")
    {
        value = AssetSourceTextureSemantic::ColorGradingLut;
        return true;
    }
    if (text == "Debug")
    {
        value = AssetSourceTextureSemantic::Debug;
        return true;
    }
    return false;
}

bool parseTextureColorSpace(const std::string& text, AssetSourceTextureColorSpace& value) noexcept
{
    if (text == "Srgb")
    {
        value = AssetSourceTextureColorSpace::Srgb;
        return true;
    }
    if (text == "Linear")
    {
        value = AssetSourceTextureColorSpace::Linear;
        return true;
    }
    if (text == "EncodedNormal")
    {
        value = AssetSourceTextureColorSpace::EncodedNormal;
        return true;
    }
    return false;
}

bool parseMaterialModel(const std::string& text, AssetSourceMaterialModel& value) noexcept
{
    if (text == "Basic")
    {
        value = AssetSourceMaterialModel::Basic;
        return true;
    }
    if (text == "TerrainSplat")
    {
        value = AssetSourceMaterialModel::TerrainSplat;
        return true;
    }
    return false;
}

bool parseMaterialAlphaMode(const std::string& text, AssetSourceMaterialAlphaMode& value) noexcept
{
    if (text == "Opaque")
    {
        value = AssetSourceMaterialAlphaMode::Opaque;
        return true;
    }
    if (text == "AlphaTest")
    {
        value = AssetSourceMaterialAlphaMode::AlphaTest;
        return true;
    }
    if (text == "AlphaBlend")
    {
        value = AssetSourceMaterialAlphaMode::AlphaBlend;
        return true;
    }
    return false;
}

bool sameBounds(const AssetSourceBounds& lhs, const AssetSourceBounds& rhs) noexcept
{
    for (int axis = 0; axis < 3; ++axis)
    {
        if (lhs.min[axis] != rhs.min[axis] || lhs.max[axis] != rhs.max[axis])
        {
            return false;
        }
    }
    return true;
}

bool matchesMeshDescriptor(
    const LoadedMeshAsset& mesh,
    const AssetSourceMeshDescriptor& descriptor) noexcept
{
    return mesh.vertices.size() == descriptor.vertexCount &&
        mesh.indices.size() == descriptor.indexCount &&
        sameBounds(mesh.localBounds, descriptor.localBounds);
}

bool matchesTextureDescriptor(
    const LoadedTextureAsset& texture,
    const AssetSourceTextureDescriptor& descriptor) noexcept
{
    return texture.width == descriptor.width &&
        texture.height == descriptor.height &&
        texture.mipCount == descriptor.mipCount &&
        texture.format == descriptor.format &&
        texture.semantic == descriptor.semantic &&
        texture.colorSpace == descriptor.colorSpace;
}

bool matchesMaterialDescriptor(
    const LoadedMaterialAsset& material,
    const AssetSourceMaterialDescriptor& descriptor) noexcept
{
    if (material.model != descriptor.model ||
        material.alphaMode != descriptor.alphaMode ||
        material.textureRefCount != descriptor.textureRefCount)
    {
        return false;
    }

    for (std::uint32_t index = 0; index < material.textureRefCount; ++index)
    {
        if (!(material.textureRefs[index] == descriptor.textureRefs[index]))
        {
            return false;
        }
    }
    return true;
}

LoadedAssetImportResult makeStatus(const LoadedAssetImportStatus status)
{
    LoadedAssetImportResult result;
    result.status = status;
    return result;
}

bool parseMagic(
    std::istringstream& line,
    const char* expectedMagic,
    bool& magicSeen)
{
    std::string version;
    if (magicSeen || !(line >> version) || version != "1" || !requireEnd(line))
    {
        return false;
    }
    (void)expectedMagic;
    magicSeen = true;
    return true;
}

bool parseMeshLine(const std::string& text, MeshDevData& data)
{
    std::istringstream line(text);
    std::string token;
    if (!(line >> token))
    {
        return false;
    }

    if (token == "fmeshdev")
    {
        return parseMagic(line, "fmeshdev", data.magicSeen);
    }
    if (token == "counts")
    {
        if (data.countsSeen ||
            !readUint32(line, data.declaredVertexCount) ||
            !readUint32(line, data.declaredIndexCount) ||
            !requireEnd(line))
        {
            return false;
        }
        data.countsSeen = true;
        return true;
    }
    if (token == "bounds")
    {
        if (data.boundsSeen ||
            !readFloat(line, data.mesh.localBounds.min[0]) ||
            !readFloat(line, data.mesh.localBounds.min[1]) ||
            !readFloat(line, data.mesh.localBounds.min[2]) ||
            !readFloat(line, data.mesh.localBounds.max[0]) ||
            !readFloat(line, data.mesh.localBounds.max[1]) ||
            !readFloat(line, data.mesh.localBounds.max[2]) ||
            !requireEnd(line))
        {
            return false;
        }
        data.boundsSeen = true;
        return true;
    }
    if (token == "v")
    {
        LoadedMeshVertex vertex;
        if (!readFloat(line, vertex.position[0]) ||
            !readFloat(line, vertex.position[1]) ||
            !readFloat(line, vertex.position[2]) ||
            !readFloat(line, vertex.normal[0]) ||
            !readFloat(line, vertex.normal[1]) ||
            !readFloat(line, vertex.normal[2]) ||
            !readFloat(line, vertex.colorLinear[0]) ||
            !readFloat(line, vertex.colorLinear[1]) ||
            !readFloat(line, vertex.colorLinear[2]) ||
            !readFloat(line, vertex.colorLinear[3]) ||
            !requireEnd(line))
        {
            return false;
        }
        data.mesh.vertices.push_back(vertex);
        return true;
    }
    if (token == "i")
    {
        std::array<std::uint16_t, 3> indices = {};
        if (!readUint16(line, indices[0]) ||
            !readUint16(line, indices[1]) ||
            !readUint16(line, indices[2]) ||
            !requireEnd(line))
        {
            return false;
        }
        data.mesh.indices.push_back(indices[0]);
        data.mesh.indices.push_back(indices[1]);
        data.mesh.indices.push_back(indices[2]);
        return true;
    }

    return false;
}

bool parseTextureLine(const std::string& text, TextureDevData& data)
{
    std::istringstream line(text);
    std::string token;
    if (!(line >> token))
    {
        return false;
    }

    if (token == "ftexdev")
    {
        return parseMagic(line, "ftexdev", data.magicSeen);
    }
    if (token == "size")
    {
        if (data.sizeSeen ||
            !readUint32(line, data.texture.width) ||
            !readUint32(line, data.texture.height) ||
            !requireEnd(line))
        {
            return false;
        }
        data.sizeSeen = true;
        return true;
    }
    if (token == "mip")
    {
        if (data.mipSeen ||
            !readUint32(line, data.texture.mipCount) ||
            !requireEnd(line))
        {
            return false;
        }
        data.mipSeen = true;
        return true;
    }
    if (token == "format")
    {
        std::string value;
        if (data.formatSeen ||
            !(line >> value) ||
            !parseTextureFormat(value, data.texture.format) ||
            !requireEnd(line))
        {
            return false;
        }
        data.formatSeen = true;
        return true;
    }
    if (token == "semantic")
    {
        std::string value;
        if (data.semanticSeen ||
            !(line >> value) ||
            !parseTextureSemantic(value, data.texture.semantic) ||
            !requireEnd(line))
        {
            return false;
        }
        data.semanticSeen = true;
        return true;
    }
    if (token == "colorspace")
    {
        std::string value;
        if (data.colorSpaceSeen ||
            !(line >> value) ||
            !parseTextureColorSpace(value, data.texture.colorSpace) ||
            !requireEnd(line))
        {
            return false;
        }
        data.colorSpaceSeen = true;
        return true;
    }
    if (token == "bytes")
    {
        if (data.bytesSeen || !readUint32(line, data.declaredByteCount) ||
            data.declaredByteCount > kMaxTextureBytesForDevImport)
        {
            return false;
        }

        data.texture.bytes.clear();
        data.texture.bytes.reserve(data.declaredByteCount);
        for (std::uint32_t index = 0; index < data.declaredByteCount; ++index)
        {
            std::uint8_t value = 0;
            if (!readUint8(line, value))
            {
                return false;
            }
            data.texture.bytes.push_back(value);
        }

        if (!requireEnd(line))
        {
            return false;
        }
        data.bytesSeen = true;
        return true;
    }

    return false;
}

bool parseMaterialLine(const std::string& text, MaterialDevData& data)
{
    std::istringstream line(text);
    std::string token;
    if (!(line >> token))
    {
        return false;
    }

    if (token == "fmatdev")
    {
        return parseMagic(line, "fmatdev", data.magicSeen);
    }
    if (token == "model")
    {
        std::string value;
        if (data.modelSeen ||
            !(line >> value) ||
            !parseMaterialModel(value, data.material.model) ||
            !requireEnd(line))
        {
            return false;
        }
        data.modelSeen = true;
        return true;
    }
    if (token == "alpha")
    {
        std::string value;
        if (data.alphaSeen ||
            !(line >> value) ||
            !parseMaterialAlphaMode(value, data.material.alphaMode) ||
            !requireEnd(line))
        {
            return false;
        }
        data.alphaSeen = true;
        return true;
    }
    if (token == "textures")
    {
        if (data.texturesSeen ||
            !readUint32(line, data.material.textureRefCount) ||
            data.material.textureRefCount > kMaxAssetSourceMaterialTextureRefs)
        {
            return false;
        }

        for (std::uint32_t index = 0; index < data.material.textureRefCount; ++index)
        {
            if (!readAssetId(line, data.material.textureRefs[index]))
            {
                return false;
            }
        }

        if (!requireEnd(line))
        {
            return false;
        }
        data.texturesSeen = true;
        return true;
    }

    return false;
}

template <typename ParseLine>
bool parseLines(std::ifstream& input, ParseLine parseLine)
{
    std::string line;
    while (std::getline(input, line))
    {
        if (line.empty())
        {
            return false;
        }
        if (!parseLine(line))
        {
            return false;
        }
    }
    return true;
}

LoadedAssetImportResult importMesh(std::ifstream& input, const AssetSourceRecord& source)
{
    MeshDevData data;
    data.mesh.id = source.id;
    if (!parseLines(input, [&data](const std::string& line) { return parseMeshLine(line, data); }) ||
        !data.magicSeen ||
        !data.countsSeen ||
        !data.boundsSeen ||
        data.mesh.vertices.size() != data.declaredVertexCount ||
        data.mesh.indices.size() != data.declaredIndexCount)
    {
        return makeStatus(LoadedAssetImportStatus::ParseError);
    }

    if (!matchesMeshDescriptor(data.mesh, source.descriptor.mesh))
    {
        return makeStatus(LoadedAssetImportStatus::DescriptorMismatch);
    }

    LoadedAssetImportResult result;
    result.payload.kind = AssetKind::Mesh;
    result.payload.mesh = data.mesh;
    result.payloadValidation = validateLoadedAssetPayload(result.payload);
    result.status =
        result.payloadValidation == LoadedAssetPayloadValidationResult::Success ?
        LoadedAssetImportStatus::Success :
        LoadedAssetImportStatus::PayloadValidationFailed;
    return result;
}

LoadedAssetImportResult importTexture(std::ifstream& input, const AssetSourceRecord& source)
{
    TextureDevData data;
    data.texture.id = source.id;
    if (!parseLines(input, [&data](const std::string& line) { return parseTextureLine(line, data); }) ||
        !data.magicSeen ||
        !data.sizeSeen ||
        !data.mipSeen ||
        !data.formatSeen ||
        !data.semanticSeen ||
        !data.colorSpaceSeen ||
        !data.bytesSeen ||
        data.texture.bytes.size() != data.declaredByteCount)
    {
        return makeStatus(LoadedAssetImportStatus::ParseError);
    }

    if (!matchesTextureDescriptor(data.texture, source.descriptor.texture))
    {
        return makeStatus(LoadedAssetImportStatus::DescriptorMismatch);
    }

    LoadedAssetImportResult result;
    result.payload.kind = AssetKind::Texture;
    result.payload.texture = data.texture;
    result.payloadValidation = validateLoadedAssetPayload(result.payload);
    result.status =
        result.payloadValidation == LoadedAssetPayloadValidationResult::Success ?
        LoadedAssetImportStatus::Success :
        LoadedAssetImportStatus::PayloadValidationFailed;
    return result;
}

LoadedAssetImportResult importMaterial(std::ifstream& input, const AssetSourceRecord& source)
{
    MaterialDevData data;
    data.material.id = source.id;
    if (!parseLines(input, [&data](const std::string& line) { return parseMaterialLine(line, data); }) ||
        !data.magicSeen ||
        !data.modelSeen ||
        !data.alphaSeen ||
        !data.texturesSeen)
    {
        return makeStatus(LoadedAssetImportStatus::ParseError);
    }

    if (!matchesMaterialDescriptor(data.material, source.descriptor.material))
    {
        return makeStatus(LoadedAssetImportStatus::DescriptorMismatch);
    }

    LoadedAssetImportResult result;
    result.payload.kind = AssetKind::Material;
    result.payload.material = data.material;
    result.payloadValidation = validateLoadedAssetPayload(result.payload);
    result.status =
        result.payloadValidation == LoadedAssetPayloadValidationResult::Success ?
        LoadedAssetImportStatus::Success :
        LoadedAssetImportStatus::PayloadValidationFailed;
    return result;
}
} // namespace

const char* loadedAssetImportStatusName(const LoadedAssetImportStatus status) noexcept
{
    switch (status)
    {
    case LoadedAssetImportStatus::Success:
        return "Success";
    case LoadedAssetImportStatus::InvalidArgument:
        return "InvalidArgument";
    case LoadedAssetImportStatus::SourceValidationFailed:
        return "SourceValidationFailed";
    case LoadedAssetImportStatus::IoError:
        return "IoError";
    case LoadedAssetImportStatus::ParseError:
        return "ParseError";
    case LoadedAssetImportStatus::DescriptorMismatch:
        return "DescriptorMismatch";
    case LoadedAssetImportStatus::PayloadValidationFailed:
        return "PayloadValidationFailed";
    case LoadedAssetImportStatus::UnsupportedKind:
        return "UnsupportedKind";
    }

    return "Unknown";
}

LoadedAssetImportResult importLoadedAssetPayloadFromDevFile(
    const AssetSourceRecord& source)
{
    LoadedAssetImportResult result;
    switch (source.kind)
    {
    case AssetKind::Mesh:
    case AssetKind::Texture:
    case AssetKind::Material:
        break;
    case AssetKind::Unknown:
    case AssetKind::TerrainChunk:
    case AssetKind::Skeleton:
    case AssetKind::SkinnedMesh:
    case AssetKind::Shader:
        result.status = LoadedAssetImportStatus::UnsupportedKind;
        return result;
    }

    result.sourceValidation = validateAssetSourceRecord(source);
    if (result.sourceValidation != AssetSourceRecordValidationResult::Success)
    {
        result.status = LoadedAssetImportStatus::SourceValidationFailed;
        return result;
    }

    std::ifstream input(source.uri);
    if (!input)
    {
        result.status = LoadedAssetImportStatus::IoError;
        return result;
    }

    switch (source.kind)
    {
    case AssetKind::Mesh:
        result = importMesh(input, source);
        break;
    case AssetKind::Texture:
        result = importTexture(input, source);
        break;
    case AssetKind::Material:
        result = importMaterial(input, source);
        break;
    case AssetKind::Unknown:
    case AssetKind::TerrainChunk:
    case AssetKind::Skeleton:
    case AssetKind::SkinnedMesh:
    case AssetKind::Shader:
        result.status = LoadedAssetImportStatus::UnsupportedKind;
        break;
    }

    result.sourceValidation = AssetSourceRecordValidationResult::Success;
    return result;
}
} // namespace full_engine
