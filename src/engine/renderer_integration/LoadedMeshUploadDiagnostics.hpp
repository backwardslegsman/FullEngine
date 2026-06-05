#pragma once

#include "engine/assets/LoadedAssetPayload.hpp"

#include <cstddef>
#include <cstdint>

namespace full_engine
{
/** @brief Detailed renderer-upload contract status for one loaded mesh payload. */
enum class LoadedMeshUploadDiagnosticStatus
{
    Valid,
    InvalidLoadedPayload,
    CountOverflow,
    InvalidRendererStructure,
    InvalidRendererVertexData,
    InvalidRendererIndices,
    DegenerateTriangles,
};

/**
 * @brief First concrete mesh upload contract failure found during diagnostics.
 *
 * Indices use `invalidIndex` when the corresponding concept does not apply.
 * The value is copied diagnostics only; it does not retain pointers into mesh
 * data and does not own renderer resources.
 */
struct LoadedMeshUploadDiagnosticFailure
{
    static constexpr std::uint32_t invalidIndex = UINT32_MAX;

    /** @brief First invalid vertex index, or `invalidIndex` when not applicable. */
    std::uint32_t vertexIndex = invalidIndex;

    /** @brief First invalid index-buffer element offset, or `invalidIndex`. */
    std::uint32_t indexOffset = invalidIndex;

    /** @brief First degenerate triangle ordinal, or `invalidIndex`. */
    std::uint32_t triangleIndex = invalidIndex;
};

/** @brief Value-only upload contract diagnostics for one loaded mesh payload. */
struct LoadedMeshUploadDiagnostics
{
    /** @brief Overall diagnostic status. */
    LoadedMeshUploadDiagnosticStatus status = LoadedMeshUploadDiagnosticStatus::InvalidLoadedPayload;

    /** @brief Renderer-free payload validation detail. */
    LoadedAssetPayloadValidationResult payloadValidation =
        LoadedAssetPayloadValidationResult::Success;

    /** @brief Number of vertices in the loaded mesh payload. */
    std::size_t vertexCount = 0;

    /** @brief Number of indices in the loaded mesh payload. */
    std::size_t indexCount = 0;

    /** @brief Count of vertices that fail renderer vertex-data validation. */
    std::size_t invalidVertexDataCount = 0;

    /** @brief Count of index elements that reference vertices out of range. */
    std::size_t invalidIndexCount = 0;

    /** @brief Count of zero-area or non-finite triangles rejected by the renderer contract. */
    std::size_t degenerateTriangleCount = 0;

    /** @brief First concrete failure location where one is available. */
    LoadedMeshUploadDiagnosticFailure firstFailure = {};
};

/** @brief Returns a stable diagnostic name for a loaded mesh upload diagnostic status. */
const char* loadedMeshUploadDiagnosticStatusName(
    LoadedMeshUploadDiagnosticStatus status) noexcept;

/**
 * @brief Diagnoses whether one loaded mesh payload can satisfy the renderer mesh upload contract.
 *
 * The helper mirrors the current renderer mesh descriptor validation used by
 * `LoadedAssetUploadPlan`, but returns finer-grained counters for tooling. It
 * performs no renderer calls, file IO, handle lookup, resource creation,
 * resource destruction, mesh cleanup, or mutation of the source payload.
 */
LoadedMeshUploadDiagnostics diagnoseLoadedMeshUploadContract(
    const LoadedMeshAsset& mesh) noexcept;
} // namespace full_engine
