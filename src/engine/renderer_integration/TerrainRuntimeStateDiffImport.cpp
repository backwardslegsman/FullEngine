#include "engine/renderer_integration/TerrainRuntimeStateDiffImport.hpp"

#include <cctype>
#include <cstdint>
#include <fstream>
#include <limits>
#include <string>

namespace full_engine
{
namespace
{
void countChange(TerrainRuntimeStateDiffSummary& summary, const TerrainRuntimeStateChangeType type) noexcept
{
    switch (type)
    {
    case TerrainRuntimeStateChangeType::Added:
        ++summary.addedCount;
        break;
    case TerrainRuntimeStateChangeType::Removed:
        ++summary.removedCount;
        break;
    case TerrainRuntimeStateChangeType::ReadinessChanged:
        ++summary.readinessChangedCount;
        break;
    case TerrainRuntimeStateChangeType::ResidencyChanged:
        ++summary.residencyChangedCount;
        break;
    case TerrainRuntimeStateChangeType::HandlePresenceChanged:
        ++summary.handlePresenceChangedCount;
        break;
    }
}

bool parseChangeType(const std::string& value, TerrainRuntimeStateChangeType& type) noexcept
{
    if (value == "Added")
    {
        type = TerrainRuntimeStateChangeType::Added;
        return true;
    }
    if (value == "Removed")
    {
        type = TerrainRuntimeStateChangeType::Removed;
        return true;
    }
    if (value == "ReadinessChanged")
    {
        type = TerrainRuntimeStateChangeType::ReadinessChanged;
        return true;
    }
    if (value == "ResidencyChanged")
    {
        type = TerrainRuntimeStateChangeType::ResidencyChanged;
        return true;
    }
    if (value == "HandlePresenceChanged")
    {
        type = TerrainRuntimeStateChangeType::HandlePresenceChanged;
        return true;
    }

    return false;
}

bool parseReadiness(const std::string& value, TerrainRuntimeChunkReadiness& readiness) noexcept
{
    if (value == "Renderable")
    {
        readiness = TerrainRuntimeChunkReadiness::Renderable;
        return true;
    }
    if (value == "MissingRegistry")
    {
        readiness = TerrainRuntimeChunkReadiness::MissingRegistry;
        return true;
    }
    if (value == "MissingWorldDesc")
    {
        readiness = TerrainRuntimeChunkReadiness::MissingWorldDesc;
        return true;
    }
    if (value == "MissingResources")
    {
        readiness = TerrainRuntimeChunkReadiness::MissingResources;
        return true;
    }
    if (value == "NotResident")
    {
        readiness = TerrainRuntimeChunkReadiness::NotResident;
        return true;
    }
    if (value == "MissingTerrainHandle")
    {
        readiness = TerrainRuntimeChunkReadiness::MissingTerrainHandle;
        return true;
    }

    return false;
}

bool parseResidency(const std::string& value, ChunkResidencyState& state) noexcept
{
    if (value == "Unloaded")
    {
        state = ChunkResidencyState::Unloaded;
        return true;
    }
    if (value == "Loading")
    {
        state = ChunkResidencyState::Loading;
        return true;
    }
    if (value == "Resident")
    {
        state = ChunkResidencyState::Resident;
        return true;
    }
    if (value == "Unloading")
    {
        state = ChunkResidencyState::Unloading;
        return true;
    }

    return false;
}

bool parseStringField(const std::string& line, const char* name, std::string& value)
{
    const std::string prefix = std::string("\"") + name + "\":\"";
    const std::size_t start = line.find(prefix);
    if (start == std::string::npos)
    {
        return false;
    }

    const std::size_t valueStart = start + prefix.size();
    const std::size_t valueEnd = line.find('"', valueStart);
    if (valueEnd == std::string::npos)
    {
        return false;
    }

    value = line.substr(valueStart, valueEnd - valueStart);
    return true;
}

bool parseSignedField(const std::string& line, const char* name, std::int32_t& value)
{
    const std::string prefix = std::string("\"") + name + "\":";
    const std::size_t start = line.find(prefix);
    if (start == std::string::npos)
    {
        return false;
    }

    std::size_t cursor = start + prefix.size();
    if (cursor >= line.size())
    {
        return false;
    }

    bool negative = false;
    if (line[cursor] == '-')
    {
        negative = true;
        ++cursor;
    }

    if (cursor >= line.size() || !std::isdigit(static_cast<unsigned char>(line[cursor])))
    {
        return false;
    }

    std::int64_t parsed = 0;
    const std::int64_t positiveLimit = negative
        ? -static_cast<std::int64_t>(std::numeric_limits<std::int32_t>::min())
        : static_cast<std::int64_t>(std::numeric_limits<std::int32_t>::max());
    while (cursor < line.size() && std::isdigit(static_cast<unsigned char>(line[cursor])))
    {
        const std::int64_t digit = static_cast<std::int64_t>(line[cursor] - '0');
        if (parsed > (positiveLimit - digit) / 10)
        {
            return false;
        }
        parsed = parsed * 10 + digit;
        ++cursor;
    }

    if (cursor >= line.size() || (line[cursor] != ',' && line[cursor] != '}'))
    {
        return false;
    }

    value = static_cast<std::int32_t>(negative ? -parsed : parsed);
    return true;
}

bool parseBoolField(const std::string& line, const char* name, bool& value)
{
    const std::string prefix = std::string("\"") + name + "\":";
    const std::size_t start = line.find(prefix);
    if (start == std::string::npos)
    {
        return false;
    }

    const std::size_t valueStart = start + prefix.size();
    if (line.compare(valueStart, 4, "true") == 0)
    {
        const std::size_t valueEnd = valueStart + 4;
        if (valueEnd < line.size() && (line[valueEnd] == ',' || line[valueEnd] == '}'))
        {
            value = true;
            return true;
        }
    }
    if (line.compare(valueStart, 5, "false") == 0)
    {
        const std::size_t valueEnd = valueStart + 5;
        if (valueEnd < line.size() && (line[valueEnd] == ',' || line[valueEnd] == '}'))
        {
            value = false;
            return true;
        }
    }

    return false;
}

bool parseChange(const std::string& line, TerrainRuntimeStateChange& change)
{
    if (line.size() < 2 || line.front() != '{' || line.back() != '}')
    {
        return false;
    }

    std::string type;
    std::string previousReadiness;
    std::string currentReadiness;
    std::string previousResidency;
    std::string currentResidency;

    return
        parseSignedField(line, "x", change.id.x) &&
        parseSignedField(line, "y", change.id.y) &&
        parseSignedField(line, "z", change.id.z) &&
        parseStringField(line, "type", type) &&
        parseChangeType(type, change.type) &&
        parseStringField(line, "previousReadiness", previousReadiness) &&
        parseReadiness(previousReadiness, change.previousReadiness) &&
        parseStringField(line, "currentReadiness", currentReadiness) &&
        parseReadiness(currentReadiness, change.currentReadiness) &&
        parseStringField(line, "previousResidency", previousResidency) &&
        parseResidency(previousResidency, change.previousResidency) &&
        parseStringField(line, "currentResidency", currentResidency) &&
        parseResidency(currentResidency, change.currentResidency) &&
        parseBoolField(line, "previousHasTerrainHandle", change.previousHasTerrainHandle) &&
        parseBoolField(line, "currentHasTerrainHandle", change.currentHasTerrainHandle);
}
} // namespace

const char* terrainRuntimeStateDiffImportResultName(const TerrainRuntimeStateDiffImportResult result) noexcept
{
    switch (result)
    {
    case TerrainRuntimeStateDiffImportResult::Success:
        return "Success";
    case TerrainRuntimeStateDiffImportResult::InvalidArgument:
        return "InvalidArgument";
    case TerrainRuntimeStateDiffImportResult::IoError:
        return "IoError";
    case TerrainRuntimeStateDiffImportResult::ParseError:
        return "ParseError";
    }

    return "Unknown";
}

TerrainRuntimeStateDiffImport importTerrainRuntimeStateDiffJsonLines(const char* path)
{
    TerrainRuntimeStateDiffImport imported;
    if (path == nullptr || path[0] == '\0')
    {
        imported.result = TerrainRuntimeStateDiffImportResult::InvalidArgument;
        return imported;
    }

    std::ifstream input(path);
    if (!input.is_open())
    {
        imported.result = TerrainRuntimeStateDiffImportResult::IoError;
        return imported;
    }

    std::string line;
    while (std::getline(input, line))
    {
        TerrainRuntimeStateChange change;
        if (!parseChange(line, change))
        {
            imported.diff = {};
            imported.result = TerrainRuntimeStateDiffImportResult::ParseError;
            return imported;
        }
        countChange(imported.diff.summary, change.type);
        imported.diff.changes.push_back(change);
    }

    if (input.bad())
    {
        imported.diff = {};
        imported.result = TerrainRuntimeStateDiffImportResult::IoError;
        return imported;
    }

    imported.result = TerrainRuntimeStateDiffImportResult::Success;
    return imported;
}
} // namespace full_engine
