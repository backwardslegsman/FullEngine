#pragma once

#include "engine/renderer_integration/TerrainRuntimeController.hpp"

#include <vector>

namespace full_engine
{
/** @brief Result of exporting terrain runtime events to a diagnostics file. */
enum class TerrainRuntimeEventExportResult
{
    Success,
    InvalidArgument,
    IoError,
};

/** @brief Returns a stable diagnostic name for a terrain runtime event export result. */
const char* terrainRuntimeEventExportResultName(TerrainRuntimeEventExportResult result) noexcept;

/**
 * @brief Exports retained terrain runtime events as deterministic JSON Lines.
 *
 * Each line is one compact JSON object with copied diagnostic counters. The
 * exporter writes CPU-side diagnostics only and does not touch renderer state,
 * queues, registries, resources, handles, or frame data.
 */
TerrainRuntimeEventExportResult exportTerrainRuntimeEventsJsonLines(
    const TerrainRuntimeEventLog& log,
    const char* path);

/**
 * @brief Exports a chronological terrain runtime event snapshot as JSON Lines.
 *
 * The supplied event vector is read only. Events are written in vector order.
 */
TerrainRuntimeEventExportResult exportTerrainRuntimeEventsJsonLines(
    const std::vector<TerrainRuntimeEvent>& events,
    const char* path);
} // namespace full_engine
