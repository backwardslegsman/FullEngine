#pragma once

#include "engine/renderer_integration/TerrainRuntimeStateDiff.hpp"

#include <vector>

namespace full_engine
{
/** @brief Result of exporting terrain runtime state diff diagnostics. */
enum class TerrainRuntimeStateDiffExportResult
{
    Success,
    InvalidArgument,
    IoError,
};

/** @brief Returns a stable diagnostic name for a terrain runtime state diff export result. */
const char* terrainRuntimeStateDiffExportResultName(TerrainRuntimeStateDiffExportResult result) noexcept;

/**
 * @brief Exports a terrain runtime state diff as deterministic JSON Lines.
 *
 * Each line is one changed chunk, written in the diff's stored order. The
 * exported schema contains copied chunk coordinates, change type, readiness,
 * residency, and terrain-handle-presence fields. The exporter writes CPU-side
 * diagnostics only and does not touch renderer state, queues, registries,
 * resources, handles, frame data, or sample UI state.
 *
 * @return `InvalidArgument` for a null or empty path, `IoError` for open/write
 * failures, otherwise `Success`.
 */
TerrainRuntimeStateDiffExportResult exportTerrainRuntimeStateDiffJsonLines(
    const TerrainRuntimeStateDiff& diff,
    const char* path);

/**
 * @brief Exports terrain runtime state change records as JSON Lines.
 *
 * The supplied vector is read only. Changes are written in vector order and
 * copied into the output stream; the caller retains ownership of all data.
 *
 * @return `InvalidArgument` for a null or empty path, `IoError` for open/write
 * failures, otherwise `Success`.
 */
TerrainRuntimeStateDiffExportResult exportTerrainRuntimeStateDiffJsonLines(
    const std::vector<TerrainRuntimeStateChange>& changes,
    const char* path);
} // namespace full_engine
