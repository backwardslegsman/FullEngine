#include "engine/renderer_integration/TerrainSubmissionAdapter.hpp"
#include "engine/renderer_integration/TerrainResourceCatalog.hpp"

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

full_renderer::TerrainChunkHandle terrainHandle(const std::uint32_t id, const std::uint32_t generation)
{
    return full_renderer::TerrainChunkHandle{id, generation};
}

full_engine::RenderBounds bounds(const float minX)
{
    full_engine::RenderBounds result;
    result.min = {minX, 0.0f, 0.0f};
    result.max = {minX + 10.0f, 10.0f, 10.0f};
    return result;
}

full_engine::TerrainRendererCommand command(
    const full_engine::ChunkId& id,
    const full_engine::TerrainRendererCommandType type,
    const full_renderer::TerrainChunkHandle handle = {})
{
    full_engine::TerrainRendererCommand result;
    result.id = id;
    result.type = type;
    result.bounds = bounds(static_cast<float>(id.x));
    result.handle = handle;
    return result;
}

full_engine::TerrainChunkResourceDesc resources(const full_engine::ChunkId& id)
{
    full_engine::TerrainChunkResourceDesc result;
    result.id = id;
    result.lodCount = 1;
    result.lods[0].mesh = full_renderer::MeshHandle{101};
    result.lods[0].material = full_renderer::MaterialHandle{201};
    result.lods[0].maxDistanceMeters = 100.0f;
    return result;
}

bool sameId(const full_engine::ChunkId& lhs, const full_engine::ChunkId& rhs)
{
    return lhs == rhs;
}

class FakeRenderer final : public full_renderer::IRenderer
{
public:
    full_renderer::TerrainChunkHandle nextCreateHandle = terrainHandle(1, 1);
    full_renderer::RendererResult nextUpdateResult = full_renderer::RendererResult::Success;
    int createCalls = 0;
    int updateCalls = 0;
    int destroyCalls = 0;
    full_renderer::TerrainChunkHandle lastUpdateHandle = {};
    full_renderer::TerrainChunkHandle lastDestroyHandle = {};

    full_renderer::RendererResult initialize(const full_renderer::RendererInitDesc&) override
    {
        return full_renderer::RendererResult::Success;
    }

    void shutdown() noexcept override {}

    bool isInitialized() const noexcept override
    {
        return true;
    }

    full_renderer::MeshHandle createMesh(const full_renderer::MeshDesc&) override
    {
        return {};
    }

    void destroyMesh(full_renderer::MeshHandle) noexcept override {}

    full_renderer::SkeletonHandle createSkeleton(const full_renderer::SkeletonDesc&) override
    {
        return {};
    }

    void destroySkeleton(full_renderer::SkeletonHandle) noexcept override {}

    full_renderer::SkinnedMeshHandle createSkinnedMesh(const full_renderer::SkinnedMeshDesc&) override
    {
        return {};
    }

    void destroySkinnedMesh(full_renderer::SkinnedMeshHandle) noexcept override {}

    full_renderer::TextureHandle createTexture(const full_renderer::TextureDesc&) override
    {
        return {};
    }

    void destroyTexture(full_renderer::TextureHandle) noexcept override {}

    full_renderer::MaterialHandle createMaterial(const full_renderer::MaterialDesc&) override
    {
        return {};
    }

    void destroyMaterial(full_renderer::MaterialHandle) noexcept override {}

    full_renderer::TerrainChunkHandle createTerrainChunk(const full_renderer::TerrainChunkDesc&) override
    {
        ++createCalls;
        return nextCreateHandle;
    }

    full_renderer::RendererResult updateTerrainChunk(
        const full_renderer::TerrainChunkHandle handle,
        const full_renderer::TerrainChunkDesc&) override
    {
        ++updateCalls;
        lastUpdateHandle = handle;
        return nextUpdateResult;
    }

    void destroyTerrainChunk(const full_renderer::TerrainChunkHandle handle) noexcept override
    {
        ++destroyCalls;
        lastDestroyHandle = handle;
    }

    full_renderer::RendererResult resize(const full_renderer::RendererResizeDesc&) override
    {
        return full_renderer::RendererResult::Success;
    }

    full_renderer::RendererResult beginFrame(const full_renderer::FrameDesc&) override
    {
        return full_renderer::RendererResult::Success;
    }

    full_renderer::RendererResult submit(const full_renderer::RenderPacket&) override
    {
        return full_renderer::RendererResult::Success;
    }

    full_renderer::RendererResult endFrame() override
    {
        return full_renderer::RendererResult::Success;
    }

    full_renderer::RendererStats getStats() const noexcept override
    {
        return {};
    }

    full_renderer::TerrainStats getTerrainStats() const noexcept override
    {
        return {};
    }

    std::uint32_t copyTerrainDebugInfo(full_renderer::TerrainChunkDebugInfo*, std::uint32_t) const noexcept override
    {
        return 0;
    }

    std::uint32_t copyTerrainBatchDebugInfo(full_renderer::TerrainBatchDebugInfo*, std::uint32_t) const noexcept override
    {
        return 0;
    }

    std::uint32_t copyTerrainShadowCasterDebugInfo(
        full_renderer::TerrainChunkDebugInfo*,
        std::uint32_t) const noexcept override
    {
        return 0;
    }
};

full_engine::TerrainDescriptorBuildResult descriptorsFor(const full_engine::TerrainRendererCommandList& commands)
{
    full_engine::TerrainResourceCatalog catalog;
    for (const full_engine::TerrainRendererCommand& item : commands.commands)
    {
        if (item.type == full_engine::TerrainRendererCommandType::CreateChunk ||
            item.type == full_engine::TerrainRendererCommandType::UpdateChunk)
        {
            catalog.addChunkResources(resources(item.id));
        }
    }

    return full_engine::buildTerrainDescriptors(commands, catalog);
}

void testCreateMapsReturnedHandle(std::vector<std::string>& failures)
{
    const full_engine::ChunkId id{1, 0, 0};
    full_engine::TerrainRendererCommandList commands;
    commands.commands.push_back(command(id, full_engine::TerrainRendererCommandType::CreateChunk));
    full_engine::TerrainDescriptorBuildResult descriptors = descriptorsFor(commands);

    FakeRenderer renderer;
    renderer.nextCreateHandle = terrainHandle(55, 2);
    full_engine::ChunkTerrainHandleMap handles;

    const full_engine::TerrainSubmissionResult result =
        full_engine::submitTerrainCommands(renderer, descriptors, commands, handles);

    expect(result.operations.size() == 1, "create produces one submission op", failures);
    expect(result.operations[0].status == full_engine::TerrainSubmissionOpStatus::Created, "valid create reports created", failures);
    expect(renderer.createCalls == 1, "create calls renderer", failures);
    expect(handles.contains(id), "create maps returned handle", failures);
    expect(handles.findHandle(id) != nullptr && handles.findHandle(id)->id == 55, "mapped handle matches renderer return", failures);
    expect(result.summary.createdCount == 1, "created summary increments", failures);
}

void testCreateInvalidHandleFailsWithoutMapMutation(std::vector<std::string>& failures)
{
    const full_engine::ChunkId id{2, 0, 0};
    full_engine::TerrainRendererCommandList commands;
    commands.commands.push_back(command(id, full_engine::TerrainRendererCommandType::CreateChunk));
    full_engine::TerrainDescriptorBuildResult descriptors = descriptorsFor(commands);

    FakeRenderer renderer;
    renderer.nextCreateHandle = {};
    full_engine::ChunkTerrainHandleMap handles;

    const full_engine::TerrainSubmissionResult result =
        full_engine::submitTerrainCommands(renderer, descriptors, commands, handles);

    expect(result.operations[0].status == full_engine::TerrainSubmissionOpStatus::RendererFailed, "invalid create handle reports renderer failure", failures);
    expect(renderer.createCalls == 1, "failed create still calls renderer", failures);
    expect(!handles.contains(id), "failed create does not mutate map", failures);
    expect(result.summary.rendererFailedCount == 1, "renderer failure summary increments", failures);
}

void testCreateDuplicateMappingReportsHandleMapFailure(std::vector<std::string>& failures)
{
    const full_engine::ChunkId id{3, 0, 0};
    full_engine::TerrainRendererCommandList commands;
    commands.commands.push_back(command(id, full_engine::TerrainRendererCommandType::CreateChunk));
    full_engine::TerrainDescriptorBuildResult descriptors = descriptorsFor(commands);

    FakeRenderer renderer;
    renderer.nextCreateHandle = terrainHandle(77, 4);
    full_engine::ChunkTerrainHandleMap handles;
    handles.mapChunk(id, terrainHandle(1, 1));

    const full_engine::TerrainSubmissionResult result =
        full_engine::submitTerrainCommands(renderer, descriptors, commands, handles);

    expect(result.operations[0].status == full_engine::TerrainSubmissionOpStatus::HandleMapFailed, "duplicate create mapping reports map failure", failures);
    expect(renderer.createCalls == 1, "duplicate map failure happens after renderer create", failures);
    expect(handles.findHandle(id) != nullptr && handles.findHandle(id)->id == 1, "duplicate map failure preserves existing handle", failures);
    expect(result.summary.handleMapFailedCount == 1, "handle map failure summary increments", failures);
}

void testUpdateSuccessAndFailure(std::vector<std::string>& failures)
{
    const full_engine::ChunkId id{4, 0, 0};
    const full_renderer::TerrainChunkHandle existing = terrainHandle(4, 9);
    full_engine::TerrainRendererCommandList commands;
    commands.commands.push_back(command(id, full_engine::TerrainRendererCommandType::UpdateChunk, existing));
    full_engine::TerrainDescriptorBuildResult descriptors = descriptorsFor(commands);

    FakeRenderer renderer;
    full_engine::ChunkTerrainHandleMap handles;
    handles.mapChunk(id, existing);

    full_engine::TerrainSubmissionResult result =
        full_engine::submitTerrainCommands(renderer, descriptors, commands, handles);

    expect(result.operations[0].status == full_engine::TerrainSubmissionOpStatus::Updated, "successful update reports updated", failures);
    expect(renderer.updateCalls == 1, "update calls renderer", failures);
    expect(renderer.lastUpdateHandle.id == existing.id, "update passes command handle", failures);
    expect(handles.findHandle(id) != nullptr && handles.findHandle(id)->id == existing.id, "successful update does not mutate map", failures);

    renderer.nextUpdateResult = full_renderer::RendererResult::InvalidArgument;
    result = full_engine::submitTerrainCommands(renderer, descriptors, commands, handles);

    expect(result.operations[0].status == full_engine::TerrainSubmissionOpStatus::RendererFailed, "failed update reports renderer failure", failures);
    expect(result.operations[0].rendererResult == full_renderer::RendererResult::InvalidArgument, "failed update records renderer result", failures);
}

void testDestroyRemovesMapEntry(std::vector<std::string>& failures)
{
    const full_engine::ChunkId id{5, 0, 0};
    const full_renderer::TerrainChunkHandle existing = terrainHandle(5, 1);
    full_engine::TerrainRendererCommandList commands;
    commands.commands.push_back(command(id, full_engine::TerrainRendererCommandType::DestroyChunk, existing));
    const full_engine::TerrainDescriptorBuildResult descriptors;

    FakeRenderer renderer;
    full_engine::ChunkTerrainHandleMap handles;
    handles.mapChunk(id, existing);

    const full_engine::TerrainSubmissionResult result =
        full_engine::submitTerrainCommands(renderer, descriptors, commands, handles);

    expect(result.operations[0].status == full_engine::TerrainSubmissionOpStatus::Destroyed, "destroy reports destroyed", failures);
    expect(renderer.destroyCalls == 1, "destroy calls renderer", failures);
    expect(renderer.lastDestroyHandle.id == existing.id, "destroy passes command handle", failures);
    expect(!handles.contains(id), "destroy removes handle map entry", failures);
    expect(result.summary.destroyedCount == 1, "destroy summary increments", failures);
}

void testDestroyInvalidHandleSkips(std::vector<std::string>& failures)
{
    full_engine::TerrainRendererCommandList commands;
    commands.commands.push_back(command({6, 0, 0}, full_engine::TerrainRendererCommandType::DestroyChunk, {}));

    FakeRenderer renderer;
    full_engine::ChunkTerrainHandleMap handles;
    const full_engine::TerrainDescriptorBuildResult descriptors;

    const full_engine::TerrainSubmissionResult result =
        full_engine::submitTerrainCommands(renderer, descriptors, commands, handles);

    expect(result.operations[0].status == full_engine::TerrainSubmissionOpStatus::Skipped, "invalid destroy handle skips", failures);
    expect(renderer.destroyCalls == 0, "invalid destroy handle does not call renderer", failures);
}

void testKeepAndNonReadyDescriptorSkip(std::vector<std::string>& failures)
{
    full_engine::TerrainRendererCommandList commands;
    commands.commands.push_back(command({7, 0, 0}, full_engine::TerrainRendererCommandType::KeepChunk, terrainHandle(7, 1)));
    commands.commands.push_back(command({8, 0, 0}, full_engine::TerrainRendererCommandType::CreateChunk));

    full_engine::TerrainDescriptorBuildResult descriptors;
    full_engine::TerrainDescriptorIntent keepIntent;
    keepIntent.id = {7, 0, 0};
    keepIntent.sourceCommand = full_engine::TerrainRendererCommandType::KeepChunk;
    keepIntent.status = full_engine::TerrainDescriptorBuildStatus::IgnoredCommand;
    descriptors.intents.push_back(keepIntent);
    full_engine::TerrainDescriptorIntent missingIntent;
    missingIntent.id = {8, 0, 0};
    missingIntent.sourceCommand = full_engine::TerrainRendererCommandType::CreateChunk;
    missingIntent.status = full_engine::TerrainDescriptorBuildStatus::MissingResources;
    descriptors.intents.push_back(missingIntent);

    FakeRenderer renderer;
    full_engine::ChunkTerrainHandleMap handles;

    const full_engine::TerrainSubmissionResult result =
        full_engine::submitTerrainCommands(renderer, descriptors, commands, handles);

    expect(result.operations.size() == 2, "keep and skipped create produce two ops", failures);
    expect(result.operations[0].status == full_engine::TerrainSubmissionOpStatus::Kept, "keep command reports kept", failures);
    expect(result.operations[1].status == full_engine::TerrainSubmissionOpStatus::Skipped, "non-ready create descriptor skips", failures);
    expect(renderer.createCalls == 0 && renderer.updateCalls == 0 && renderer.destroyCalls == 0, "keep and skipped descriptor do not call renderer", failures);
    expect(result.summary.keptCount == 1, "kept summary increments", failures);
    expect(result.summary.skippedCount == 1, "skipped summary increments", failures);
}

void testOperationOrder(std::vector<std::string>& failures)
{
    full_engine::TerrainRendererCommandList commands;
    commands.commands.push_back(command({10, 0, 0}, full_engine::TerrainRendererCommandType::CreateChunk));
    commands.commands.push_back(command({11, 0, 0}, full_engine::TerrainRendererCommandType::KeepChunk, terrainHandle(11, 1)));
    commands.commands.push_back(command({12, 0, 0}, full_engine::TerrainRendererCommandType::DestroyChunk, terrainHandle(12, 1)));
    full_engine::TerrainDescriptorBuildResult descriptors = descriptorsFor(commands);

    FakeRenderer renderer;
    renderer.nextCreateHandle = terrainHandle(10, 1);
    full_engine::ChunkTerrainHandleMap handles;
    handles.mapChunk({12, 0, 0}, terrainHandle(12, 1));

    const full_engine::TerrainSubmissionResult result =
        full_engine::submitTerrainCommands(renderer, descriptors, commands, handles);

    expect(result.operations.size() == 3, "mixed commands produce three ops", failures);
    expect(sameId(result.operations[0].id, {10, 0, 0}), "first op preserves command order", failures);
    expect(sameId(result.operations[1].id, {11, 0, 0}), "second op preserves command order", failures);
    expect(sameId(result.operations[2].id, {12, 0, 0}), "third op preserves command order", failures);
}
} // namespace

int main()
{
    std::vector<std::string> failures;

    testCreateMapsReturnedHandle(failures);
    testCreateInvalidHandleFailsWithoutMapMutation(failures);
    testCreateDuplicateMappingReportsHandleMapFailure(failures);
    testUpdateSuccessAndFailure(failures);
    testDestroyRemovesMapEntry(failures);
    testDestroyInvalidHandleSkips(failures);
    testKeepAndNonReadyDescriptorSkip(failures);
    testOperationOrder(failures);

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
