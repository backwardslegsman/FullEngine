#pragma once

#include "engine/renderer_integration/TerrainRuntimeStateDiff.hpp"

namespace full_engine
{
/** @brief Result status for importing terrain runtime state diff diagnostics. */
enum class TerrainRuntimeStateDiffImportResult
{
    Success,
    InvalidArgument,
    IoError,
    ParseError,
};

/**
 * @brief Imported terrain runtime state diff plus the import status.
 *
 * On parse or IO failure, `diff` is empty. On success, `diff.changes` preserves
 * file order and `diff.summary` is rebuilt from the imported change types.
 */
struct TerrainRuntimeStateDiffImport
{
    TerrainRuntimeStateDiffImportResult result = TerrainRuntimeStateDiffImportResult::Success;
    TerrainRuntimeStateDiff diff = {};
};

/** @brief Returns a stable diagnostic name for a terrain runtime state diff import result. */
const char* terrainRuntimeStateDiffImportResultName(TerrainRuntimeStateDiffImportResult result) noexcept;

/**
 * @brief Imports terrain runtime state diffs from the JSON Lines format written by the exporter.
 *
 * The importer reads CPU-side diagnostic records only. It preserves file order,
 * ignores unknown fields, rebuilds summary counters, and does not mutate queues,
 * registries, resources, renderer handles, frame data, or sample UI state.
 * Required flat-schema fields must be present and valid; malformed records
 * return `ParseError` without partial output.
 *
 * @return Import status plus the copied diff records when successful.
 */
TerrainRuntimeStateDiffImport importTerrainRuntimeStateDiffJsonLines(const char* path);
} // namespace full_engine
