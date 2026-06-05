#include "engine/assets/GltfMaterialSlotAudit.hpp"

#include <algorithm>
#include <fstream>
#include <iterator>

namespace full_engine
{
namespace
{
struct RawSlotKey
{
    AssetSourceMaterialTextureSlot slot = AssetSourceMaterialTextureSlot::Unknown;
    const char* key = "";
};

constexpr RawSlotKey kRawSlotKeys[] = {
    {AssetSourceMaterialTextureSlot::BaseColor, "baseColorTexture"},
    {AssetSourceMaterialTextureSlot::Normal, "normalTexture"},
    {AssetSourceMaterialTextureSlot::MetallicRoughness, "metallicRoughnessTexture"},
    {AssetSourceMaterialTextureSlot::Occlusion, "occlusionTexture"},
    {AssetSourceMaterialTextureSlot::Emissive, "emissiveTexture"},
};

bool containsAssetId(const AssetId* const ids, const std::size_t count, const AssetId id) noexcept
{
    if (ids == nullptr)
    {
        return false;
    }
    for (std::size_t index = 0; index < count; ++index)
    {
        if (ids[index] == id)
        {
            return true;
        }
    }
    return false;
}

bool importedTexturePayloadContains(
    const LoadedAssetPayload* const payloads,
    const std::size_t count,
    const AssetId id) noexcept
{
    if (payloads == nullptr)
    {
        return false;
    }
    for (std::size_t index = 0; index < count; ++index)
    {
        if (payloads[index].kind == AssetKind::Texture &&
            payloads[index].texture.id == id)
        {
            return true;
        }
    }
    return false;
}

bool isShaderActiveSlot(const AssetSourceMaterialTextureSlot slot) noexcept
{
    return slot == AssetSourceMaterialTextureSlot::BaseColor ||
        slot == AssetSourceMaterialTextureSlot::Normal;
}

bool isSourceMetadataForSlot(
    const AssetSourceRecord& source,
    const AssetSourceMaterialTextureSlot slot) noexcept
{
    if (source.kind != AssetKind::Texture)
    {
        return false;
    }

    switch (slot)
    {
    case AssetSourceMaterialTextureSlot::BaseColor:
    case AssetSourceMaterialTextureSlot::Emissive:
        return source.descriptor.texture.semantic == AssetSourceTextureSemantic::Color &&
            source.descriptor.texture.colorSpace == AssetSourceTextureColorSpace::Srgb;
    case AssetSourceMaterialTextureSlot::Normal:
        return source.descriptor.texture.semantic == AssetSourceTextureSemantic::NormalMap &&
            source.descriptor.texture.colorSpace == AssetSourceTextureColorSpace::EncodedNormal;
    case AssetSourceMaterialTextureSlot::MetallicRoughness:
    case AssetSourceMaterialTextureSlot::Occlusion:
        return source.descriptor.texture.semantic == AssetSourceTextureSemantic::LinearData &&
            source.descriptor.texture.colorSpace == AssetSourceTextureColorSpace::Linear;
    case AssetSourceMaterialTextureSlot::Unknown:
        break;
    }
    return false;
}

void addCounts(
    GltfMaterialSlotAuditSlotCounts& destination,
    const GltfMaterialSlotAuditSlotCounts& source) noexcept
{
    destination.rawTextureKeyCount += source.rawTextureKeyCount;
    destination.extractedRefCount += source.extractedRefCount;
    destination.emittedTextureSourceCount += source.emittedTextureSourceCount;
    destination.importedTexturePayloadCount += source.importedTexturePayloadCount;
    destination.resolvedTextureRefCount += source.resolvedTextureRefCount;
    destination.shaderActiveRefCount += source.shaderActiveRefCount;
}

std::string readFile(const std::string& uri)
{
    std::ifstream input(uri);
    if (!input)
    {
        return {};
    }
    return {
        std::istreambuf_iterator<char>(input),
        std::istreambuf_iterator<char>()};
}

std::size_t findMatchingBracket(
    const std::string& text,
    const std::size_t openOffset,
    const char open,
    const char close) noexcept
{
    bool inString = false;
    bool escaped = false;
    std::uint32_t depth = 0;
    for (std::size_t offset = openOffset; offset < text.size(); ++offset)
    {
        const char ch = text[offset];
        if (inString)
        {
            if (escaped)
            {
                escaped = false;
            }
            else if (ch == '\\')
            {
                escaped = true;
            }
            else if (ch == '"')
            {
                inString = false;
            }
            continue;
        }

        if (ch == '"')
        {
            inString = true;
        }
        else if (ch == open)
        {
            ++depth;
        }
        else if (ch == close)
        {
            if (depth == 0)
            {
                return std::string::npos;
            }
            --depth;
            if (depth == 0)
            {
                return offset;
            }
        }
    }
    return std::string::npos;
}

std::vector<std::string> extractRawMaterialObjects(const std::string& text)
{
    const std::size_t materialsKey = text.find("\"materials\"");
    if (materialsKey == std::string::npos)
    {
        return {};
    }

    const std::size_t arrayOpen = text.find('[', materialsKey);
    if (arrayOpen == std::string::npos)
    {
        return {};
    }
    const std::size_t arrayClose = findMatchingBracket(text, arrayOpen, '[', ']');
    if (arrayClose == std::string::npos)
    {
        return {};
    }

    std::vector<std::string> materials;
    std::size_t cursor = arrayOpen + 1;
    while (cursor < arrayClose)
    {
        const std::size_t objectOpen = text.find('{', cursor);
        if (objectOpen == std::string::npos || objectOpen >= arrayClose)
        {
            break;
        }
        const std::size_t objectClose = findMatchingBracket(text, objectOpen, '{', '}');
        if (objectClose == std::string::npos || objectClose > arrayClose)
        {
            break;
        }
        materials.push_back(text.substr(objectOpen, objectClose - objectOpen + 1));
        cursor = objectClose + 1;
    }
    return materials;
}

std::uint32_t countRawKey(const std::string& material, const char* const key) noexcept
{
    return material.find(key) != std::string::npos ? 1U : 0U;
}

GltfMaterialSlotAuditRecord* findOrCreateRecord(
    GltfMaterialSlotAudit& audit,
    const std::uint32_t materialIndex)
{
    const auto existing = std::find_if(
        audit.records.begin(),
        audit.records.end(),
        [materialIndex](const GltfMaterialSlotAuditRecord& record)
        {
            return record.materialIndex == materialIndex;
        });
    if (existing != audit.records.end())
    {
        return &*existing;
    }

    GltfMaterialSlotAuditRecord record;
    record.materialIndex = materialIndex;
    audit.records.push_back(record);
    return &audit.records.back();
}
} // namespace

std::size_t gltfMaterialSlotAuditIndex(const AssetSourceMaterialTextureSlot slot) noexcept
{
    switch (slot)
    {
    case AssetSourceMaterialTextureSlot::BaseColor:
        return 0;
    case AssetSourceMaterialTextureSlot::Normal:
        return 1;
    case AssetSourceMaterialTextureSlot::MetallicRoughness:
        return 2;
    case AssetSourceMaterialTextureSlot::Occlusion:
        return 3;
    case AssetSourceMaterialTextureSlot::Emissive:
        return 4;
    case AssetSourceMaterialTextureSlot::Unknown:
        break;
    }
    return kGltfMaterialSlotAuditSlotCount;
}

const char* gltfMaterialSlotAuditSlotName(const std::size_t slotIndex) noexcept
{
    switch (slotIndex)
    {
    case 0:
        return "BaseColor";
    case 1:
        return "Normal";
    case 2:
        return "MetallicRoughness";
    case 3:
        return "Occlusion";
    case 4:
        return "Emissive";
    }
    return "Unknown";
}

GltfMaterialSlotAudit auditGltfMaterialSlots(
    const std::string& uri,
    const GltfMaterialAssetImportOptions& options,
    const GltfMaterialAssetImportResult* const importResult,
    const LoadedAssetPayload* const importedTexturePayloads,
    const std::size_t importedTexturePayloadCount,
    const AssetId* const resolvedTextureIds,
    const std::size_t resolvedTextureIdCount)
{
    GltfMaterialSlotAudit audit;
    const GltfMaterialAssetImportResult localImport =
        importResult == nullptr ? importGltfMaterialAssetSources(uri, options) : GltfMaterialAssetImportResult{};
    const GltfMaterialAssetImportResult& materials =
        importResult != nullptr ? *importResult : localImport;
    audit.importStatus = materials.status;

    const std::vector<std::string> rawMaterials = extractRawMaterialObjects(readFile(uri));
    for (std::size_t materialIndex = 0; materialIndex < rawMaterials.size(); ++materialIndex)
    {
        GltfMaterialSlotAuditRecord* const record = findOrCreateRecord(
            audit,
            static_cast<std::uint32_t>(materialIndex));
        for (const RawSlotKey& rawSlot : kRawSlotKeys)
        {
            const std::size_t slotIndex = gltfMaterialSlotAuditIndex(rawSlot.slot);
            record->slots[slotIndex].rawTextureKeyCount += countRawKey(rawMaterials[materialIndex], rawSlot.key);
        }
    }

    for (const GltfMaterialAssetImportRecord& imported : materials.records)
    {
        GltfMaterialSlotAuditRecord* const record = findOrCreateRecord(audit, imported.materialIndex);
        record->materialId = imported.materialId;
        record->extracted = imported.status == GltfMaterialAssetImportRecordStatus::Planned ||
            imported.status == GltfMaterialAssetImportRecordStatus::NoBaseColorTexture;

        for (std::uint32_t refIndex = 0; refIndex < imported.textureRefCount; ++refIndex)
        {
            const AssetSourceMaterialTextureRef& ref = imported.textureRefs[refIndex];
            const std::size_t slotIndex = gltfMaterialSlotAuditIndex(ref.slot);
            if (slotIndex >= kGltfMaterialSlotAuditSlotCount)
            {
                continue;
            }

            GltfMaterialSlotAuditSlotCounts& counts = record->slots[slotIndex];
            ++counts.extractedRefCount;
            if (importedTexturePayloadContains(
                    importedTexturePayloads,
                    importedTexturePayloadCount,
                    ref.id))
            {
                ++counts.importedTexturePayloadCount;
            }
            if (containsAssetId(resolvedTextureIds, resolvedTextureIdCount, ref.id))
            {
                ++counts.resolvedTextureRefCount;
            }
            if (isShaderActiveSlot(ref.slot))
            {
                ++counts.shaderActiveRefCount;
            }
        }
    }

    for (const AssetSourceRecord& source : materials.sourceRecords)
    {
        for (const RawSlotKey& rawSlot : kRawSlotKeys)
        {
            if (!isSourceMetadataForSlot(source, rawSlot.slot))
            {
                continue;
            }
            const std::size_t slotIndex = gltfMaterialSlotAuditIndex(rawSlot.slot);
            for (GltfMaterialSlotAuditRecord& record : audit.records)
            {
                const auto imported = std::find_if(
                    materials.records.begin(),
                    materials.records.end(),
                    [&record](const GltfMaterialAssetImportRecord& materialRecord)
                    {
                        return materialRecord.materialIndex == record.materialIndex;
                    });
                if (imported == materials.records.end())
                {
                    continue;
                }
                for (std::uint32_t refIndex = 0; refIndex < imported->textureRefCount; ++refIndex)
                {
                    if (imported->textureRefs[refIndex].id == source.id &&
                        imported->textureRefs[refIndex].slot == rawSlot.slot)
                    {
                        ++record.slots[slotIndex].emittedTextureSourceCount;
                    }
                }
            }
        }
    }

    std::sort(
        audit.records.begin(),
        audit.records.end(),
        [](const GltfMaterialSlotAuditRecord& lhs, const GltfMaterialSlotAuditRecord& rhs)
        {
            return lhs.materialIndex < rhs.materialIndex;
        });

    for (const GltfMaterialSlotAuditRecord& record : audit.records)
    {
        for (std::size_t slotIndex = 0; slotIndex < kGltfMaterialSlotAuditSlotCount; ++slotIndex)
        {
            addCounts(audit.slots[slotIndex], record.slots[slotIndex]);
        }
    }
    return audit;
}
} // namespace full_engine
