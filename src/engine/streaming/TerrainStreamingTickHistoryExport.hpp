#pragma once

#include "engine/streaming/TerrainStreamingLoopState.hpp"

#include <vector>

namespace full_engine
{
/** @brief Result of exporting retained terrain streaming tick history. */
enum class TerrainStreamingTickHistoryExportResult
{
    Success,
    InvalidArgument,
    IoError,
};

/** @brief Returns a stable diagnostic name for a streaming tick-history export result. */
const char* terrainStreamingTickHistoryExportResultName(
    TerrainStreamingTickHistoryExportResult result) noexcept;

/**
 * @brief Exports retained streaming tick history as deterministic JSON Lines.
 *
 * Each line is one flat JSON object with copied streaming, queue, runtime,
 * lifecycle, and submission counters. The exporter performs synchronous file
 * IO only; it does not mutate the history, loop state, terrain runtime queues,
 * registries, catalogs, renderer handles, renderer resources, jobs, manifests,
 * or sample/editor state.
 *
 * @param history Caller-owned retained tick history. Read only during the call.
 * @param path Null-terminated output path. The file is truncated or created.
 * @return Success when all retained events were written, InvalidArgument for a
 * null or empty path, and IoError when the file cannot be opened or written.
 */
TerrainStreamingTickHistoryExportResult exportTerrainStreamingTickHistoryJsonLines(
    const TerrainStreamingTickHistory& history,
    const char* path);

/**
 * @brief Exports a chronological streaming tick event snapshot as JSON Lines.
 *
 * Events are written in vector order. The vector is read only and not retained.
 */
TerrainStreamingTickHistoryExportResult exportTerrainStreamingTickHistoryJsonLines(
    const std::vector<TerrainStreamingTickEvent>& events,
    const char* path);
} // namespace full_engine
