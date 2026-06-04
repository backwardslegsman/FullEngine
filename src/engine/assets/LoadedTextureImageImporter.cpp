#include "engine/assets/LoadedTextureImageImporter.hpp"

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include <cstdint>
#include <fstream>
#include <limits>

namespace full_engine
{
namespace
{
constexpr int kRequiredChannelCount = 4;

bool matchesTextureDescriptor(
    const LoadedTextureAsset& texture,
    const AssetSourceTextureDescriptor& descriptor) noexcept
{
    return texture.width == descriptor.width &&
        texture.height == descriptor.height &&
        texture.mipCount == descriptor.mipCount &&
        texture.format == descriptor.format;
}

} // namespace

const char* loadedTextureImageImportStatusName(
    const LoadedTextureImageImportStatus status) noexcept
{
    switch (status)
    {
    case LoadedTextureImageImportStatus::Success:
        return "Success";
    case LoadedTextureImageImportStatus::InvalidArgument:
        return "InvalidArgument";
    case LoadedTextureImageImportStatus::SourceValidationFailed:
        return "SourceValidationFailed";
    case LoadedTextureImageImportStatus::IoError:
        return "IoError";
    case LoadedTextureImageImportStatus::DecodeError:
        return "DecodeError";
    case LoadedTextureImageImportStatus::DescriptorMismatch:
        return "DescriptorMismatch";
    case LoadedTextureImageImportStatus::PayloadValidationFailed:
        return "PayloadValidationFailed";
    case LoadedTextureImageImportStatus::UnsupportedKind:
        return "UnsupportedKind";
    }

    return "Unknown";
}

LoadedTextureImageImportResult importLoadedTexturePayloadFromImageFile(
    const AssetSourceRecord& source)
{
    LoadedTextureImageImportResult result;
    if (source.kind != AssetKind::Texture)
    {
        result.status = LoadedTextureImageImportStatus::UnsupportedKind;
        return result;
    }

    result.sourceValidation = validateAssetSourceRecord(source);
    if (result.sourceValidation != AssetSourceRecordValidationResult::Success)
    {
        result.status = LoadedTextureImageImportStatus::SourceValidationFailed;
        return result;
    }

    std::ifstream input(source.uri, std::ios::binary);
    if (!input)
    {
        result.status = LoadedTextureImageImportStatus::IoError;
        return result;
    }

    int width = 0;
    int height = 0;
    int sourceChannels = 0;
    stbi_uc* const decoded = stbi_load(
        source.uri.c_str(),
        &width,
        &height,
        &sourceChannels,
        kRequiredChannelCount);
    (void)sourceChannels;
    if (decoded == nullptr || width <= 0 || height <= 0)
    {
        stbi_image_free(decoded);
        result.status = LoadedTextureImageImportStatus::DecodeError;
        return result;
    }

    const std::uint64_t byteCount =
        static_cast<std::uint64_t>(width) *
        static_cast<std::uint64_t>(height) *
        static_cast<std::uint64_t>(kRequiredChannelCount);
    if (byteCount > static_cast<std::uint64_t>(std::numeric_limits<std::size_t>::max()))
    {
        stbi_image_free(decoded);
        result.status = LoadedTextureImageImportStatus::DecodeError;
        return result;
    }

    result.payload.kind = AssetKind::Texture;
    result.payload.texture.id = source.id;
    result.payload.texture.width = static_cast<std::uint32_t>(width);
    result.payload.texture.height = static_cast<std::uint32_t>(height);
    result.payload.texture.mipCount = 1;
    result.payload.texture.format = AssetSourceTextureFormat::Rgba8;
    result.payload.texture.semantic = source.descriptor.texture.semantic;
    result.payload.texture.colorSpace = source.descriptor.texture.colorSpace;
    result.payload.texture.bytes.assign(decoded, decoded + static_cast<std::size_t>(byteCount));
    stbi_image_free(decoded);

    if (!matchesTextureDescriptor(result.payload.texture, source.descriptor.texture))
    {
        result.status = LoadedTextureImageImportStatus::DescriptorMismatch;
        return result;
    }

    result.payloadValidation = validateLoadedAssetPayload(result.payload);
    result.status =
        result.payloadValidation == LoadedAssetPayloadValidationResult::Success ?
        LoadedTextureImageImportStatus::Success :
        LoadedTextureImageImportStatus::PayloadValidationFailed;
    return result;
}
} // namespace full_engine
