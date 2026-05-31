#include "engine/streaming/TerrainStreamingPlanner.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <map>
#include <set>

namespace full_engine
{
namespace
{
bool isSetupPresent(const TerrainRuntimeChunkState& state) noexcept
{
    return state.hasRegistry && state.hasWorldDesc && state.hasResources;
}

bool hasAnySetupState(const TerrainRuntimeChunkState& state) noexcept
{
    return state.hasRegistry || state.hasWorldDesc || state.hasResources;
}

TerrainRuntimeChunkState stateFor(
    const std::map<ChunkId, TerrainRuntimeChunkState>& states,
    const ChunkId& id)
{
    const auto found = states.find(id);
    if (found != states.end())
    {
        return found->second;
    }

    TerrainRuntimeChunkState missing;
    missing.id = id;
    return missing;
}

void countOperation(TerrainStreamingPlanSummary& summary, const TerrainStreamingChunkAction action) noexcept
{
    switch (action)
    {
    case TerrainStreamingChunkAction::AddSetup:
        ++summary.addSetupCount;
        break;
    case TerrainStreamingChunkAction::KeepSetup:
        ++summary.keepSetupCount;
        break;
    case TerrainStreamingChunkAction::RemoveSetup:
        ++summary.removeSetupCount;
        break;
    case TerrainStreamingChunkAction::MakeResident:
        ++summary.makeResidentCount;
        break;
    case TerrainStreamingChunkAction::KeepResident:
        ++summary.keepResidentCount;
        break;
    case TerrainStreamingChunkAction::MakeUnloaded:
        ++summary.makeUnloadedCount;
        break;
    case TerrainStreamingChunkAction::KeepUnloaded:
        ++summary.keepUnloadedCount;
        break;
    case TerrainStreamingChunkAction::Skipped:
        ++summary.skippedCount;
        break;
    }
}

void appendOperation(
    TerrainStreamingPlan& plan,
    const TerrainRuntimeChunkState& state,
    const TerrainStreamingChunkAction action,
    const bool desiredLoad,
    const bool desiredResident)
{
    TerrainStreamingPlanOp op;
    op.id = state.id;
    op.action = action;
    op.desiredLoad = desiredLoad;
    op.desiredResident = desiredResident;
    op.hasRegistry = state.hasRegistry;
    op.hasWorldDesc = state.hasWorldDesc;
    op.hasResources = state.hasResources;
    op.hasTerrainHandle = state.hasTerrainHandle;
    op.residency = state.residency;
    op.readiness = state.readiness;

    plan.operations.push_back(op);
    countOperation(plan.summary, action);
}

bool floorToInt32(const double value, std::int32_t& out) noexcept
{
    const double floored = std::floor(value);
    if (!std::isfinite(floored) ||
        floored < static_cast<double>(std::numeric_limits<std::int32_t>::min()) ||
        floored > static_cast<double>(std::numeric_limits<std::int32_t>::max()))
    {
        return false;
    }

    out = static_cast<std::int32_t>(floored);
    return true;
}

bool cameraChunkForConfig(
    const TerrainStreamingPlannerConfig& config,
    const WorldPosition& cameraWorld,
    ChunkId& outCameraChunk) noexcept
{
    if (config.chunkSizeMeters <= 0.0 ||
        !std::isfinite(config.chunkSizeMeters) ||
        config.loadRadiusChunks < 0 ||
        config.residentRadiusChunks < 0 ||
        config.residentRadiusChunks > config.loadRadiusChunks ||
        !isFinite(cameraWorld))
    {
        return false;
    }

    std::int32_t chunkX = 0;
    std::int32_t chunkZ = 0;
    if (!floorToInt32(cameraWorld.x / config.chunkSizeMeters, chunkX) ||
        !floorToInt32(cameraWorld.z / config.chunkSizeMeters, chunkZ))
    {
        return false;
    }

    outCameraChunk = ChunkId{chunkX, 0, chunkZ};
    return true;
}

std::int64_t absoluteDistance(const std::int32_t a, const std::int32_t b) noexcept
{
    const std::int64_t delta = static_cast<std::int64_t>(a) - static_cast<std::int64_t>(b);
    return delta < 0 ? -delta : delta;
}

std::int64_t chebyshevDistanceXZ(const ChunkId& lhs, const ChunkId& rhs) noexcept
{
    return std::max(absoluteDistance(lhs.x, rhs.x), absoluteDistance(lhs.z, rhs.z));
}

TerrainStreamingPlan invalidKnownPlan(
    const ChunkId* knownIds,
    const std::size_t knownIdCount,
    const TerrainRuntimeStateSnapshot& current)
{
    TerrainStreamingPlan plan;
    plan.summary.invalidInputCount = knownIdCount;
    if (knownIds == nullptr)
    {
        plan.summary.skippedCount = knownIdCount;
        return plan;
    }

    std::map<ChunkId, TerrainRuntimeChunkState> states;
    for (const TerrainRuntimeChunkState& state : current.chunks)
    {
        states[state.id] = state;
    }

    plan.operations.reserve(knownIdCount);
    for (std::size_t index = 0; index < knownIdCount; ++index)
    {
        appendOperation(plan, stateFor(states, knownIds[index]), TerrainStreamingChunkAction::Skipped, false, false);
    }

    return plan;
}
} // namespace

const char* terrainStreamingChunkActionName(const TerrainStreamingChunkAction action) noexcept
{
    switch (action)
    {
    case TerrainStreamingChunkAction::AddSetup:
        return "AddSetup";
    case TerrainStreamingChunkAction::KeepSetup:
        return "KeepSetup";
    case TerrainStreamingChunkAction::RemoveSetup:
        return "RemoveSetup";
    case TerrainStreamingChunkAction::MakeResident:
        return "MakeResident";
    case TerrainStreamingChunkAction::KeepResident:
        return "KeepResident";
    case TerrainStreamingChunkAction::MakeUnloaded:
        return "MakeUnloaded";
    case TerrainStreamingChunkAction::KeepUnloaded:
        return "KeepUnloaded";
    case TerrainStreamingChunkAction::Skipped:
        return "Skipped";
    }

    return "Unknown";
}

TerrainStreamingPlan planTerrainStreaming(
    const TerrainStreamingPlannerConfig& config,
    const WorldPosition& cameraWorld,
    const ChunkId* knownIds,
    const std::size_t knownIdCount,
    const TerrainRuntimeStateSnapshot& current)
{
    TerrainStreamingPlan plan;
    if (knownIds == nullptr)
    {
        if (knownIdCount > 0)
        {
            plan.summary.invalidInputCount = knownIdCount;
            plan.summary.skippedCount = knownIdCount;
        }
        return plan;
    }

    ChunkId cameraChunk;
    if (!cameraChunkForConfig(config, cameraWorld, cameraChunk))
    {
        return invalidKnownPlan(knownIds, knownIdCount, current);
    }

    std::map<ChunkId, TerrainRuntimeChunkState> states;
    for (const TerrainRuntimeChunkState& state : current.chunks)
    {
        states[state.id] = state;
    }

    std::set<ChunkId> desiredLoad;
    std::set<ChunkId> desiredResident;
    plan.operations.reserve(knownIdCount * 2 + current.chunks.size() * 2);

    for (std::size_t index = 0; index < knownIdCount; ++index)
    {
        const ChunkId id = knownIds[index];
        const std::int64_t distance = chebyshevDistanceXZ(id, cameraChunk);
        const bool wantsLoad = distance <= static_cast<std::int64_t>(config.loadRadiusChunks);
        const bool wantsResident = distance <= static_cast<std::int64_t>(config.residentRadiusChunks);
        if (!wantsLoad)
        {
            continue;
        }

        desiredLoad.insert(id);
        if (wantsResident)
        {
            desiredResident.insert(id);
        }

        const TerrainRuntimeChunkState state = stateFor(states, id);
        appendOperation(
            plan,
            state,
            isSetupPresent(state) ? TerrainStreamingChunkAction::KeepSetup : TerrainStreamingChunkAction::AddSetup,
            wantsLoad,
            wantsResident);

        if (wantsResident)
        {
            appendOperation(
                plan,
                state,
                state.residency == ChunkResidencyState::Resident
                    ? TerrainStreamingChunkAction::KeepResident
                    : TerrainStreamingChunkAction::MakeResident,
                wantsLoad,
                wantsResident);
        }
        else
        {
            appendOperation(
                plan,
                state,
                state.residency == ChunkResidencyState::Unloaded
                    ? TerrainStreamingChunkAction::KeepUnloaded
                    : TerrainStreamingChunkAction::MakeUnloaded,
                wantsLoad,
                wantsResident);
        }
    }

    for (const auto& entry : states)
    {
        const TerrainRuntimeChunkState& state = entry.second;
        if (desiredLoad.find(state.id) != desiredLoad.end())
        {
            continue;
        }

        if (state.hasRegistry && state.residency != ChunkResidencyState::Unloaded)
        {
            appendOperation(plan, state, TerrainStreamingChunkAction::MakeUnloaded, false, false);
        }
        if (hasAnySetupState(state))
        {
            appendOperation(plan, state, TerrainStreamingChunkAction::RemoveSetup, false, false);
        }
    }

    return plan;
}
} // namespace full_engine
