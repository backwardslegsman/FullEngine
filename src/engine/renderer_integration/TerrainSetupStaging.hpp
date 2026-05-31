#pragma once

#include "engine/renderer_integration/TerrainChunkRequests.hpp"

#include <cstddef>
#include <vector>

namespace full_engine
{
class TerrainRuntimeState;

/**
 * @brief Desired terrain setup state for one chunk before request application.
 *
 * The descriptors are copied into staging operations and request queues when
 * needed. The planner does not take ownership of renderer handles referenced by
 * `resourceDesc`; those handles remain externally owned renderer resources.
 */
struct TerrainSetupStageDesc
{
    ChunkId id = {};
    WorldChunkDesc worldDesc = {};
    TerrainChunkResourceDesc resourceDesc = {};
};

/**
 * @brief Dry-run terrain setup action selected by the staging planner.
 *
 * `ChangedUnsupported` is reported when the desired descriptors differ from
 * live setup state. This slice intentionally blocks replacement instead of
 * defining handle-safe live descriptor mutation.
 */
enum class TerrainSetupStageAction
{
    Add,
    Keep,
    Remove,
    ChangedUnsupported,
};

/**
 * @brief One dry-run terrain setup operation with current owner presence flags.
 *
 * For `Add` operations, `worldDesc` and `resourceDesc` contain the descriptors
 * to queue. For `Remove` operations, only `id` and the presence flags are
 * meaningful.
 */
struct TerrainSetupStageOp
{
    ChunkId id = {};
    TerrainSetupStageAction action = TerrainSetupStageAction::Keep;
    WorldChunkDesc worldDesc = {};
    TerrainChunkResourceDesc resourceDesc = {};
    bool hasRegistry = false;
    bool hasWorldDesc = false;
    bool hasResources = false;
};

/** @brief Summary counters for a terrain setup staging plan. */
struct TerrainSetupStageSummary
{
    std::size_t addCount = 0;
    std::size_t keepCount = 0;
    std::size_t removeCount = 0;
    std::size_t changedUnsupportedCount = 0;
};

/** @brief Ordered dry-run plan for terrain setup add/remove intent. */
struct TerrainSetupStagePlan
{
    std::vector<TerrainSetupStageOp> operations;
    TerrainSetupStageSummary summary;
};

/** @brief Result for safely queueing a terrain setup staging plan. */
enum class TerrainSetupStageQueueResult
{
    Success,
    BlockedUnsupportedChanges,
};

/** @brief Summary of requests queued or skipped from a staging plan. */
struct TerrainSetupStageQueueSummary
{
    std::size_t queuedSetupCount = 0;
    std::size_t queuedMakeResidentCount = 0;
    std::size_t skippedKeepCount = 0;
    std::size_t skippedChangedCount = 0;
};

/** @brief Result of converting a staging plan into runtime queue intent. */
struct TerrainSetupStageQueueApplyResult
{
    TerrainSetupStageQueueResult result = TerrainSetupStageQueueResult::Success;
    TerrainSetupStageQueueSummary summary;
};

/**
 * @brief Compares desired terrain setup records against current setup owners.
 *
 * The planner is CPU-only and read-only. It does not mutate registries,
 * catalogs, terrain resource catalogs, request queues, renderer handles, or
 * renderer resources. Desired records are processed in caller order, and
 * removals are appended afterward in deterministic chunk order.
 */
TerrainSetupStagePlan planTerrainSetupChanges(
    const WorldChunkRegistry& registry,
    const WorldChunkCatalog& worldCatalog,
    const TerrainResourceCatalog& resources,
    const TerrainSetupStageDesc* desired,
    std::size_t desiredCount);

/**
 * @brief Converts add/remove staging operations into a terrain setup request queue.
 *
 * `Keep` and `ChangedUnsupported` operations are intentionally skipped because
 * this slice does not define live descriptor replacement policy.
 */
TerrainChunkRequestQueue buildTerrainChunkRequestsFromStagePlan(
    const TerrainSetupStagePlan& plan);

/**
 * @brief Queues safe add/remove staging operations into terrain runtime state.
 *
 * The helper queues intent only. Runtime mutation still happens later through
 * `TerrainRuntimeState::update` or `updateWithSnapshot`. Any unsupported
 * descriptor change blocks the entire operation and queues nothing. When
 * `makeAddedChunksResident` is true, each queued setup add is paired with a
 * make-resident request so the next runtime update can create visible terrain.
 */
TerrainSetupStageQueueApplyResult queueTerrainSetupStagePlan(
    TerrainRuntimeState& runtime,
    const TerrainSetupStagePlan& plan,
    bool makeAddedChunksResident = true);
} // namespace full_engine
