#pragma once

#include "engine/renderer_integration/TerrainRuntimeController.hpp"

#include <vector>

namespace full_engine
{
/** @brief Result status for importing terrain runtime event diagnostics. */
enum class TerrainRuntimeEventImportResult
{
    Success,
    InvalidArgument,
    IoError,
    ParseError,
};

/** @brief Imported terrain runtime events plus the import status. */
struct TerrainRuntimeEventImport
{
    TerrainRuntimeEventImportResult result = TerrainRuntimeEventImportResult::Success;
    std::vector<TerrainRuntimeEvent> events;
};

/** @brief Returns a stable diagnostic name for a terrain runtime event import result. */
const char* terrainRuntimeEventImportResultName(TerrainRuntimeEventImportResult result) noexcept;

/**
 * @brief Imports terrain runtime events from the JSON Lines format written by the exporter.
 *
 * The importer reads CPU-side diagnostic counters only. It accepts the current
 * flat JSON object schema, preserves file order, ignores unknown fields, and
 * does not mutate queues, registries, resources, renderer handles, or frame
 * data.
 */
TerrainRuntimeEventImport importTerrainRuntimeEventsJsonLines(const char* path);
} // namespace full_engine
