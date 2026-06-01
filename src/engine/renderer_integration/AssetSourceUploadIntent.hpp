#pragma once

#include "engine/assets/AssetSourceCatalog.hpp"
#include "engine/renderer_integration/TerrainManifestAssetSourcePlan.hpp"
#include "full_renderer/Renderer.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace full_engine
{
/** @brief Upload-intent planning status for one source record. */
enum class AssetSourceUploadIntentStatus
{
    /** @brief Source metadata was translated into renderer-facing upload expectations. */
    Planned,

    /** @brief A source request plan record was not mapped to a source record. */
    SourceNotMapped,

    /** @brief Source record validation failed before renderer-facing translation. */
    InvalidSource,

    /** @brief Source metadata is valid, but the current renderer upload contract cannot accept it. */
    UnsupportedRendererContract,
};

/** @brief Returns a stable diagnostic name for an upload-intent status. */
const char* assetSourceUploadIntentStatusName(AssetSourceUploadIntentStatus status) noexcept;

/** @brief Renderer-facing expectation for a future mesh upload. */
struct AssetSourceMeshUploadExpectation
{
    /** @brief Expected vertex count from source metadata. */
    std::uint32_t vertexCount = 0;

    /** @brief Expected index count from source metadata. */
    std::uint32_t indexCount = 0;

    /** @brief Expected mesh-local bounds in meters. */
    full_renderer::Aabb localBounds = {};
};

/** @brief Renderer-facing expectation for a future texture upload. */
struct AssetSourceTextureUploadExpectation
{
    /** @brief Expected texture width in texels. */
    std::uint32_t width = 0;

    /** @brief Expected texture height in texels. */
    std::uint32_t height = 0;

    /** @brief Expected mip count from source metadata. */
    std::uint32_t mipCount = 0;

    /** @brief Renderer upload format corresponding to source metadata. */
    full_renderer::TextureFormat format = full_renderer::TextureFormat::Rgba8;

    /** @brief Renderer texture semantic corresponding to source metadata. */
    full_renderer::TextureSemantic semantic = full_renderer::TextureSemantic::Color;

    /** @brief Renderer texture color-space corresponding to source metadata. */
    full_renderer::TextureColorSpace colorSpace = full_renderer::TextureColorSpace::Srgb;

    /** @brief Minimum byte count expected for the current uncompressed RGBA8 upload contract. */
    std::uint64_t expectedMinimumByteCount = 0;
};

/** @brief Renderer-facing expectation for a future material upload. */
struct AssetSourceMaterialUploadExpectation
{
    /** @brief Renderer material kind corresponding to source metadata. */
    full_renderer::MaterialKind kind = full_renderer::MaterialKind::Basic;

    /** @brief Renderer material alpha policy corresponding to source metadata. */
    full_renderer::MaterialAlphaMode alphaMode = full_renderer::MaterialAlphaMode::Opaque;

    /** @brief Texture asset IDs that must be resolved to renderer handles later. */
    std::vector<AssetId> textureRefs;
};

/**
 * @brief One ordered source-to-upload-intent diagnostic record.
 *
 * The record copies source metadata and renderer-facing expectations by value.
 * It does not retain source bytes, renderer handles, renderer resources,
 * importer state, callbacks, or references to caller-owned arrays.
 */
struct AssetSourceUploadIntentRecord
{
    /** @brief Source asset ID when available. */
    AssetId id = {};

    /** @brief Source asset kind when available. */
    AssetKind kind = AssetKind::Unknown;

    /** @brief Copied opaque source URI when available. */
    std::string uri;

    /** @brief Upload-intent planning outcome. */
    AssetSourceUploadIntentStatus status = AssetSourceUploadIntentStatus::InvalidSource;

    /** @brief Mesh upload expectation when `kind` is `Mesh` and `status` is `Planned`. */
    AssetSourceMeshUploadExpectation mesh = {};

    /** @brief Texture upload expectation when `kind` is `Texture` and `status` is `Planned`. */
    AssetSourceTextureUploadExpectation texture = {};

    /** @brief Material upload expectation when `kind` is `Material` and `status` is `Planned`. */
    AssetSourceMaterialUploadExpectation material = {};
};

/** @brief Aggregate counters for an upload-intent plan. */
struct AssetSourceUploadIntentSummary
{
    /** @brief Number of source records translated successfully. */
    std::size_t plannedCount = 0;

    /** @brief Number of unmapped source request records. */
    std::size_t sourceNotMappedCount = 0;

    /** @brief Number of invalid source records. */
    std::size_t invalidSourceCount = 0;

    /** @brief Number of valid source records outside the current renderer upload contract. */
    std::size_t unsupportedRendererContractCount = 0;
};

/** @brief Ordered upload-intent planning result. */
struct AssetSourceUploadIntentPlan
{
    /** @brief One upload-intent record per supplied source or source request record. */
    std::vector<AssetSourceUploadIntentRecord> records;

    /** @brief Aggregate upload-intent counters. */
    AssetSourceUploadIntentSummary summary = {};
};

/**
 * @brief Builds renderer-facing upload expectations from source metadata.
 *
 * The helper reads source records for the duration of the call and copies
 * metadata into the returned plan. It performs no file IO, importer work,
 * byte allocation, renderer calls, renderer handle lookup, renderer resource
 * creation, or mutation of source catalogs/load state/queues.
 *
 * @param sources Caller-owned source record array. May be null only when
 * `sourceCount` is zero.
 * @param sourceCount Number of source records to inspect.
 * @return Ordered upload-intent diagnostics and summary counters.
 */
AssetSourceUploadIntentPlan buildAssetSourceUploadIntentPlan(
    const AssetSourceRecord* sources,
    std::size_t sourceCount);

/**
 * @brief Builds upload expectations from a manifest source request plan.
 *
 * Records that were not mapped to source records produce `SourceNotMapped`
 * diagnostics. Mapped records are translated in request-plan order.
 */
AssetSourceUploadIntentPlan buildAssetSourceUploadIntentPlan(
    const TerrainManifestAssetSourceRequestPlan& sourcePlan);
} // namespace full_engine
