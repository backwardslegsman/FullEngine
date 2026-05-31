#include "engine/renderer_integration/TerrainRuntimeStateDiffExport.hpp"

#include <fstream>

namespace full_engine
{
namespace
{
const char* chunkResidencyStateName(const ChunkResidencyState state) noexcept
{
    switch (state)
    {
    case ChunkResidencyState::Unloaded:
        return "Unloaded";
    case ChunkResidencyState::Loading:
        return "Loading";
    case ChunkResidencyState::Resident:
        return "Resident";
    case ChunkResidencyState::Unloading:
        return "Unloading";
    }

    return "Unknown";
}

const char* boolName(const bool value) noexcept
{
    return value ? "true" : "false";
}

void writeChange(std::ostream& stream, const TerrainRuntimeStateChange& change)
{
    stream << "{\"x\":" << change.id.x;
    stream << ",\"y\":" << change.id.y;
    stream << ",\"z\":" << change.id.z;
    stream << ",\"type\":\"" << terrainRuntimeStateChangeTypeName(change.type) << "\"";
    stream << ",\"previousReadiness\":\"" << terrainRuntimeChunkReadinessName(change.previousReadiness) << "\"";
    stream << ",\"currentReadiness\":\"" << terrainRuntimeChunkReadinessName(change.currentReadiness) << "\"";
    stream << ",\"previousResidency\":\"" << chunkResidencyStateName(change.previousResidency) << "\"";
    stream << ",\"currentResidency\":\"" << chunkResidencyStateName(change.currentResidency) << "\"";
    stream << ",\"previousHasTerrainHandle\":" << boolName(change.previousHasTerrainHandle);
    stream << ",\"currentHasTerrainHandle\":" << boolName(change.currentHasTerrainHandle);
    stream << "}\n";
}
} // namespace

const char* terrainRuntimeStateDiffExportResultName(const TerrainRuntimeStateDiffExportResult result) noexcept
{
    switch (result)
    {
    case TerrainRuntimeStateDiffExportResult::Success:
        return "Success";
    case TerrainRuntimeStateDiffExportResult::InvalidArgument:
        return "InvalidArgument";
    case TerrainRuntimeStateDiffExportResult::IoError:
        return "IoError";
    }

    return "Unknown";
}

TerrainRuntimeStateDiffExportResult exportTerrainRuntimeStateDiffJsonLines(
    const TerrainRuntimeStateDiff& diff,
    const char* path)
{
    return exportTerrainRuntimeStateDiffJsonLines(diff.changes, path);
}

TerrainRuntimeStateDiffExportResult exportTerrainRuntimeStateDiffJsonLines(
    const std::vector<TerrainRuntimeStateChange>& changes,
    const char* path)
{
    if (path == nullptr || path[0] == '\0')
    {
        return TerrainRuntimeStateDiffExportResult::InvalidArgument;
    }

    std::ofstream output(path, std::ios::out | std::ios::trunc);
    if (!output.is_open())
    {
        return TerrainRuntimeStateDiffExportResult::IoError;
    }

    for (const TerrainRuntimeStateChange& change : changes)
    {
        writeChange(output, change);
    }

    if (!output.good())
    {
        return TerrainRuntimeStateDiffExportResult::IoError;
    }

    return TerrainRuntimeStateDiffExportResult::Success;
}
} // namespace full_engine
