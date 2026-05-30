#include "engine/renderer_integration/TerrainRendererCommands.hpp"

#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

namespace
{
void expect(const bool condition, const char* message, std::vector<std::string>& failures)
{
    if (!condition)
    {
        failures.emplace_back(message);
    }
}

full_renderer::TerrainChunkHandle handle(const std::uint32_t id, const std::uint32_t generation)
{
    return full_renderer::TerrainChunkHandle{id, generation};
}

bool sameHandle(const full_renderer::TerrainChunkHandle lhs, const full_renderer::TerrainChunkHandle rhs)
{
    return lhs.id == rhs.id && lhs.generation == rhs.generation;
}

bool sameId(const full_engine::ChunkId& lhs, const full_engine::ChunkId& rhs)
{
    return lhs == rhs;
}

full_engine::RenderBounds bounds(const float minX)
{
    full_engine::RenderBounds result;
    result.min = {minX, 0.0f, 0.0f};
    result.max = {minX + 10.0f, 10.0f, 10.0f};
    return result;
}

bool sameBounds(const full_engine::RenderBounds& lhs, const full_engine::RenderBounds& rhs)
{
    return lhs.min.x == rhs.min.x &&
        lhs.min.y == rhs.min.y &&
        lhs.min.z == rhs.min.z &&
        lhs.max.x == rhs.max.x &&
        lhs.max.y == rhs.max.y &&
        lhs.max.z == rhs.max.z;
}

void addOperation(
    full_engine::TerrainLifecyclePlan& plan,
    const full_engine::ChunkId& id,
    const full_engine::TerrainLifecycleAction action,
    const float minX,
    const full_renderer::TerrainChunkHandle terrainHandle)
{
    full_engine::TerrainLifecycleOp operation;
    operation.id = id;
    operation.action = action;
    operation.bounds = bounds(minX);
    operation.handle = terrainHandle;
    plan.operations.push_back(operation);
}

void testLifecycleActionsMapToCommands(std::vector<std::string>& failures)
{
    full_engine::TerrainLifecyclePlan plan;
    addOperation(plan, {1, 0, 0}, full_engine::TerrainLifecycleAction::Create, 0.0f, {});
    addOperation(plan, {2, 0, 0}, full_engine::TerrainLifecycleAction::Keep, 20.0f, handle(20, 2));
    addOperation(plan, {3, 0, 0}, full_engine::TerrainLifecycleAction::Update, 30.0f, handle(30, 3));
    addOperation(plan, {4, 0, 0}, full_engine::TerrainLifecycleAction::Release, 40.0f, handle(40, 4));

    const full_engine::TerrainRendererCommandList commands = full_engine::buildTerrainRendererCommands(plan);

    expect(commands.commands.size() == 4, "all lifecycle operations become commands", failures);
    expect(commands.commands[0].type == full_engine::TerrainRendererCommandType::CreateChunk, "create maps to create command", failures);
    expect(commands.commands[1].type == full_engine::TerrainRendererCommandType::KeepChunk, "keep maps to keep command", failures);
    expect(commands.commands[2].type == full_engine::TerrainRendererCommandType::UpdateChunk, "update maps to update command", failures);
    expect(commands.commands[3].type == full_engine::TerrainRendererCommandType::DestroyChunk, "release maps to destroy command", failures);
}

void testOrderAndFieldsAreCopied(std::vector<std::string>& failures)
{
    full_engine::TerrainLifecyclePlan plan;
    const full_engine::ChunkId first{10, 0, 0};
    const full_engine::ChunkId second{8, 0, 0};
    const full_renderer::TerrainChunkHandle firstHandle = handle(10, 1);
    const full_renderer::TerrainChunkHandle secondHandle = handle(8, 2);
    addOperation(plan, first, full_engine::TerrainLifecycleAction::Update, 100.0f, firstHandle);
    addOperation(plan, second, full_engine::TerrainLifecycleAction::Keep, 80.0f, secondHandle);

    const full_engine::TerrainRendererCommandList commands = full_engine::buildTerrainRendererCommands(plan);

    expect(commands.commands.size() == 2, "two operations produce two commands", failures);
    expect(sameId(commands.commands[0].id, first), "first command preserves operation order", failures);
    expect(sameId(commands.commands[1].id, second), "second command preserves operation order", failures);
    expect(sameBounds(commands.commands[0].bounds, plan.operations[0].bounds), "first command copies bounds", failures);
    expect(sameBounds(commands.commands[1].bounds, plan.operations[1].bounds), "second command copies bounds", failures);
    expect(sameHandle(commands.commands[0].handle, firstHandle), "first command copies handle", failures);
    expect(sameHandle(commands.commands[1].handle, secondHandle), "second command copies handle", failures);
}

void testSummaryCountsCommandTypes(std::vector<std::string>& failures)
{
    full_engine::TerrainLifecyclePlan plan;
    addOperation(plan, {1, 0, 0}, full_engine::TerrainLifecycleAction::Create, 0.0f, {});
    addOperation(plan, {2, 0, 0}, full_engine::TerrainLifecycleAction::Create, 10.0f, {});
    addOperation(plan, {3, 0, 0}, full_engine::TerrainLifecycleAction::Keep, 20.0f, handle(3, 1));
    addOperation(plan, {4, 0, 0}, full_engine::TerrainLifecycleAction::Update, 30.0f, handle(4, 1));
    addOperation(plan, {5, 0, 0}, full_engine::TerrainLifecycleAction::Release, 40.0f, handle(5, 1));
    addOperation(plan, {6, 0, 0}, full_engine::TerrainLifecycleAction::Release, 50.0f, handle(6, 1));

    const full_engine::TerrainRendererCommandList commands = full_engine::buildTerrainRendererCommands(plan);

    expect(commands.summary.createCount == 2, "create command count is correct", failures);
    expect(commands.summary.keepCount == 1, "keep command count is correct", failures);
    expect(commands.summary.updateCount == 1, "update command count is correct", failures);
    expect(commands.summary.destroyCount == 2, "destroy command count is correct", failures);
}

void testCreateKeepsDefaultHandle(std::vector<std::string>& failures)
{
    full_engine::TerrainLifecyclePlan plan;
    addOperation(plan, {7, 0, 0}, full_engine::TerrainLifecycleAction::Create, 70.0f, {});

    const full_engine::TerrainRendererCommandList commands = full_engine::buildTerrainRendererCommands(plan);

    expect(commands.commands.size() == 1, "create operation produces one command", failures);
    expect(!full_renderer::isValid(commands.commands[0].handle), "create command preserves default invalid handle", failures);
    expect(sameBounds(commands.commands[0].bounds, plan.operations[0].bounds), "create command copies bounds", failures);
}

void testEmptyPlan(std::vector<std::string>& failures)
{
    const full_engine::TerrainLifecyclePlan plan;
    const full_engine::TerrainRendererCommandList commands = full_engine::buildTerrainRendererCommands(plan);

    expect(commands.commands.empty(), "empty lifecycle plan produces no commands", failures);
    expect(commands.summary.createCount == 0, "empty create count is zero", failures);
    expect(commands.summary.keepCount == 0, "empty keep count is zero", failures);
    expect(commands.summary.updateCount == 0, "empty update count is zero", failures);
    expect(commands.summary.destroyCount == 0, "empty destroy count is zero", failures);
}
} // namespace

int main()
{
    std::vector<std::string> failures;

    testLifecycleActionsMapToCommands(failures);
    testOrderAndFieldsAreCopied(failures);
    testSummaryCountsCommandTypes(failures);
    testCreateKeepsDefaultHandle(failures);
    testEmptyPlan(failures);

    if (!failures.empty())
    {
        for (const std::string& failure : failures)
        {
            std::cerr << failure << '\n';
        }
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
