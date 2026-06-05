#pragma once

#include "engine/assets/GltfMaterialAssetImporter.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace full_engine
{
/** @brief Number of named material texture slots tracked by the glTF material audit. */
constexpr std::size_t kGltfMaterialSlotAuditSlotCount = 5;

/**
 * @brief Counts for one named material texture slot.
 *
 * The audit is CPU-only and renderer-free. Counts are copied values and do not
 * retain pointers into glTF files, import results, payload vectors, or renderer
 * handle catalogs. "Resolved" counts are driven by caller-supplied texture
 * asset IDs; this keeps renderer handle ownership outside the asset layer.
 */
struct GltfMaterialSlotAuditSlotCounts
{
    /** @brief Number of raw glTF material objects declaring this texture key. */
    std::uint32_t rawTextureKeyCount = 0;

    /** @brief Number of extracted material texture refs for this slot. */
    std::uint32_t extractedRefCount = 0;

    /** @brief Number of emitted texture source records with this slot's metadata policy. */
    std::uint32_t emittedTextureSourceCount = 0;

    /** @brief Number of caller-provided imported texture payloads matching emitted refs. */
    std::uint32_t importedTexturePayloadCount = 0;

    /** @brief Number of extracted refs whose texture asset ID appears in caller-provided resolved IDs. */
    std::uint32_t resolvedTextureRefCount = 0;

    /** @brief Number of refs that affect the current basic shader path. */
    std::uint32_t shaderActiveRefCount = 0;
};

/** @brief Ordered audit record for one glTF material index. */
struct GltfMaterialSlotAuditRecord
{
    /** @brief Zero-based glTF material index. */
    std::uint32_t materialIndex = 0;

    /** @brief Material asset ID assigned by the supplied import options, when extracted. */
    AssetId materialId = {};

    /** @brief True when the material appears in the extractor result. */
    bool extracted = false;

    /** @brief Per-slot counters for this material. */
    std::array<GltfMaterialSlotAuditSlotCounts, kGltfMaterialSlotAuditSlotCount> slots = {};
};

/** @brief Aggregate glTF material slot audit result. */
struct GltfMaterialSlotAudit
{
    /** @brief Result of material extraction used by the audit. */
    GltfMaterialAssetImportStatus importStatus = GltfMaterialAssetImportStatus::InvalidArgument;

    /** @brief Ordered per-material audit records. */
    std::vector<GltfMaterialSlotAuditRecord> records;

    /** @brief Aggregate per-slot counters across all records. */
    std::array<GltfMaterialSlotAuditSlotCounts, kGltfMaterialSlotAuditSlotCount> slots = {};
};

/** @brief Returns the audit slot array index for a named material texture slot. */
std::size_t gltfMaterialSlotAuditIndex(
    AssetSourceMaterialTextureSlot slot) noexcept;

/** @brief Returns a stable label for a material slot audit index. */
const char* gltfMaterialSlotAuditSlotName(std::size_t slotIndex) noexcept;

/**
 * @brief Builds a CPU-only audit for glTF material texture-slot availability.
 *
 * `uri` is treated as a direct filesystem path to a glTF file. When
 * `importResult` is null, the helper runs `importGltfMaterialAssetSources`
 * using `options`; otherwise it audits the supplied import result. Imported
 * texture payloads and resolved texture IDs are optional copied observations
 * supplied by the caller. The helper performs no image decoding, renderer
 * handle lookup, renderer calls, resource creation, catalog mutation, or async
 * work.
 */
GltfMaterialSlotAudit auditGltfMaterialSlots(
    const std::string& uri,
    const GltfMaterialAssetImportOptions& options,
    const GltfMaterialAssetImportResult* importResult = nullptr,
    const LoadedAssetPayload* importedTexturePayloads = nullptr,
    std::size_t importedTexturePayloadCount = 0,
    const AssetId* resolvedTextureIds = nullptr,
    std::size_t resolvedTextureIdCount = 0);
} // namespace full_engine
