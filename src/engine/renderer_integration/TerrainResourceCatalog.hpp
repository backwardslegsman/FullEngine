#pragma once

#include "engine/world/WorldChunkRegistry.hpp"
#include "full_renderer/Terrain.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <map>

namespace full_engine
{
/** @brief One externally supplied terrain LOD resource reference for an engine chunk. */
struct TerrainResourceLod
{
    full_renderer::MeshHandle mesh = {};
    full_renderer::MaterialHandle material = {};
    float maxDistanceMeters = 0.0f;
};

/** @brief Engine-owned terrain resource contract for one chunk. */
struct TerrainChunkResourceDesc
{
    ChunkId id = {};
    std::array<TerrainResourceLod, full_renderer::kMaxTerrainLodLevels> lods = {};
    std::uint32_t lodCount = 0;
    full_renderer::TextureHandle splatMap = {};
};

/** @brief Result code for terrain resource catalog mutations. */
enum class TerrainResourceResult
{
    Success,
    AlreadyExists,
    NotFound,
    InvalidArgument,
};

/** @brief Validation result for one terrain chunk resource descriptor. */
enum class TerrainResourceValidationResult
{
    Success,
    InvalidLodCount,
    InvalidMesh,
    InvalidMaterial,
    InvalidDistance,
    UnsortedDistance,
};

/**
 * @brief Validates an engine-owned terrain chunk resource descriptor.
 *
 * This checks only public invalid renderer handle sentinels and descriptor
 * shape. It does not prove renderer resource liveness or ownership.
 */
TerrainResourceValidationResult validateTerrainChunkResources(const TerrainChunkResourceDesc& desc);

/**
 * @brief CPU-only catalog of externally supplied terrain resources keyed by `ChunkId`.
 *
 * The catalog stores descriptors by value and does not create, update, destroy,
 * submit, or validate live renderer resources. A default splat map is accepted
 * so the renderer can use its documented layer-0 fallback behavior later.
 */
class TerrainResourceCatalog
{
public:
    /** @brief Adds a valid resource descriptor for an unmapped chunk. */
    TerrainResourceResult addChunkResources(const TerrainChunkResourceDesc& desc);

    /** @brief Replaces an existing chunk resource descriptor with a valid descriptor. */
    TerrainResourceResult updateChunkResources(const TerrainChunkResourceDesc& desc);

    /** @brief Removes a chunk resource descriptor. */
    TerrainResourceResult removeChunkResources(const ChunkId& id);

    /** @brief Returns a read-only descriptor snapshot, or null if the chunk is missing. */
    const TerrainChunkResourceDesc* findChunkResources(const ChunkId& id) const;

    /** @brief Returns whether resources are currently registered for a chunk. */
    bool contains(const ChunkId& id) const;

    /** @brief Returns the number of registered chunk resource descriptors. */
    std::size_t resourceCount() const noexcept;

    /** @brief Removes all registered descriptors without touching renderer resources. */
    void clear() noexcept;

private:
    std::map<ChunkId, TerrainChunkResourceDesc> resources_;
};
} // namespace full_engine
