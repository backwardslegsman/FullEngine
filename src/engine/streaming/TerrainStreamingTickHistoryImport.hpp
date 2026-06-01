#pragma once

#include "engine/streaming/TerrainStreamingLoopState.hpp"

#include <vector>

namespace full_engine
{
/** @brief Result status for importing terrain streaming tick diagnostics. */
enum class TerrainStreamingTickHistoryImportResult
{
    Success,
    InvalidArgument,
    IoError,
    ParseError,
};

/** @brief Imported streaming tick events plus the import status. */
struct TerrainStreamingTickHistoryImport
{
    TerrainStreamingTickHistoryImportResult result =
        TerrainStreamingTickHistoryImportResult::Success;
    std::vector<TerrainStreamingTickEvent> events;
};

/** @brief Returns a stable diagnostic name for a streaming tick-history import result. */
const char* terrainStreamingTickHistoryImportResultName(
    TerrainStreamingTickHistoryImportResult result) noexcept;

/**
 * @brief Imports streaming tick events from the JSON Lines format written by the exporter.
 *
 * The importer reads CPU-side diagnostic counters only. It accepts the current
 * flat JSON object schema, preserves file order and stored sequence values,
 * ignores unknown fields, and does not mutate loop state, tick history,
 * runtime queues, registries, catalogs, renderer handles, renderer resources,
 * jobs, manifests, or sample/editor state.
 *
 * @param path Null-terminated JSON Lines path. Read during the call and not retained.
 * @return Imported event snapshot and result status. Parse and IO failures
 * return no partial events.
 */
TerrainStreamingTickHistoryImport importTerrainStreamingTickHistoryJsonLines(
    const char* path);
} // namespace full_engine
