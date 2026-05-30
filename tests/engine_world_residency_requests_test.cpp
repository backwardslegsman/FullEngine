#include "engine/world/WorldResidencyRequests.hpp"

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

void expectResidency(
    const full_engine::WorldChunkRegistry& registry,
    const full_engine::ChunkId& id,
    const full_engine::ChunkResidencyState expected,
    const char* message,
    std::vector<std::string>& failures)
{
    const full_engine::WorldChunkInfo* info = registry.findChunk(id);
    expect(info != nullptr, "expected chunk to exist for residency check", failures);
    if (info != nullptr)
    {
        expect(info->residency == expected, message, failures);
    }
}

void createChunk(full_engine::WorldChunkRegistry& registry, const full_engine::ChunkId& id)
{
    (void)registry.createChunk(id);
}

void makeResident(full_engine::WorldChunkRegistry& registry, const full_engine::ChunkId& id)
{
    (void)registry.setResidencyState(id, full_engine::ChunkResidencyState::Loading);
    (void)registry.setResidencyState(id, full_engine::ChunkResidencyState::Resident);
}

void testMakeResident(std::vector<std::string>& failures)
{
    full_engine::WorldChunkRegistry registry;
    const full_engine::ChunkId id{1, 0, 0};
    createChunk(registry, id);

    full_engine::WorldChunkResidencyRequestQueue queue;
    queue.push(id, full_engine::WorldChunkResidencyRequestType::MakeResident);
    const full_engine::WorldChunkResidencyApplyResult result = queue.applyTo(registry);

    expect(result.records.size() == 1, "make resident emits one record", failures);
    expect(result.summary.appliedCount == 1, "make resident increments applied count", failures);
    expect(result.summary.alreadySatisfiedCount == 0, "make resident has no no-op count", failures);
    expect(result.records[0].status == full_engine::WorldChunkResidencyRequestStatus::Applied, "make resident applies", failures);
    expect(result.records[0].finalState == full_engine::ChunkResidencyState::Resident, "make resident reports final state", failures);
    expectResidency(registry, id, full_engine::ChunkResidencyState::Resident, "make resident drives chunk resident", failures);
    expect(queue.requestCount() == 1, "apply does not clear queue", failures);
}

void testMakeUnloaded(std::vector<std::string>& failures)
{
    full_engine::WorldChunkRegistry registry;
    const full_engine::ChunkId id{2, 0, 0};
    createChunk(registry, id);
    makeResident(registry, id);

    full_engine::WorldChunkResidencyRequestQueue queue;
    queue.push(id, full_engine::WorldChunkResidencyRequestType::MakeUnloaded);
    const full_engine::WorldChunkResidencyApplyResult result = queue.applyTo(registry);

    expect(result.summary.appliedCount == 1, "make unloaded increments applied count", failures);
    expect(result.records[0].status == full_engine::WorldChunkResidencyRequestStatus::Applied, "make unloaded applies", failures);
    expect(result.records[0].finalState == full_engine::ChunkResidencyState::Unloaded, "make unloaded reports final state", failures);
    expectResidency(registry, id, full_engine::ChunkResidencyState::Unloaded, "make unloaded drives chunk unloaded", failures);
}

void testAlreadySatisfied(std::vector<std::string>& failures)
{
    full_engine::WorldChunkRegistry registry;
    const full_engine::ChunkId unloaded{3, 0, 0};
    const full_engine::ChunkId resident{4, 0, 0};
    createChunk(registry, unloaded);
    createChunk(registry, resident);
    makeResident(registry, resident);

    full_engine::WorldChunkResidencyRequestQueue queue;
    queue.push(unloaded, full_engine::WorldChunkResidencyRequestType::MakeUnloaded);
    queue.push(resident, full_engine::WorldChunkResidencyRequestType::MakeResident);
    const full_engine::WorldChunkResidencyApplyResult result = queue.applyTo(registry);

    expect(result.records.size() == 2, "already satisfied emits records", failures);
    expect(result.summary.appliedCount == 0, "already satisfied does not count applied", failures);
    expect(result.summary.alreadySatisfiedCount == 2, "already satisfied count is tracked", failures);
    expect(result.records[0].status == full_engine::WorldChunkResidencyRequestStatus::AlreadySatisfied, "unloaded no-op is reported", failures);
    expect(result.records[1].status == full_engine::WorldChunkResidencyRequestStatus::AlreadySatisfied, "resident no-op is reported", failures);
    expectResidency(registry, unloaded, full_engine::ChunkResidencyState::Unloaded, "unloaded no-op preserves state", failures);
    expectResidency(registry, resident, full_engine::ChunkResidencyState::Resident, "resident no-op preserves state", failures);
}

void testMissingDoesNotStopLaterRequests(std::vector<std::string>& failures)
{
    full_engine::WorldChunkRegistry registry;
    const full_engine::ChunkId missing{9, 0, 0};
    const full_engine::ChunkId valid{5, 0, 0};
    createChunk(registry, valid);

    full_engine::WorldChunkResidencyRequestQueue queue;
    queue.push(missing, full_engine::WorldChunkResidencyRequestType::MakeResident);
    queue.push(valid, full_engine::WorldChunkResidencyRequestType::MakeResident);
    const full_engine::WorldChunkResidencyApplyResult result = queue.applyTo(registry);

    expect(result.records.size() == 2, "missing and later request emit records", failures);
    expect(result.summary.notFoundCount == 1, "missing request increments not found", failures);
    expect(result.summary.appliedCount == 1, "later valid request still applies", failures);
    expect(result.records[0].status == full_engine::WorldChunkResidencyRequestStatus::NotFound, "missing request is first", failures);
    expect(result.records[1].request.id == valid, "later request order is preserved", failures);
    expectResidency(registry, valid, full_engine::ChunkResidencyState::Resident, "later request changes valid chunk", failures);
}

void testFifoAndRepeatedRequests(std::vector<std::string>& failures)
{
    full_engine::WorldChunkRegistry registry;
    const full_engine::ChunkId id{6, 0, 0};
    createChunk(registry, id);

    full_engine::WorldChunkResidencyRequestQueue queue;
    queue.push(id, full_engine::WorldChunkResidencyRequestType::MakeResident);
    queue.push(id, full_engine::WorldChunkResidencyRequestType::MakeUnloaded);
    queue.push(id, full_engine::WorldChunkResidencyRequestType::MakeResident);
    const full_engine::WorldChunkResidencyApplyResult result = queue.applyTo(registry);

    expect(result.records.size() == 3, "repeated requests emit three records", failures);
    expect(result.summary.appliedCount == 3, "repeated requests all apply", failures);
    expect(result.records[0].request.type == full_engine::WorldChunkResidencyRequestType::MakeResident, "first request order preserved", failures);
    expect(result.records[1].request.type == full_engine::WorldChunkResidencyRequestType::MakeUnloaded, "second request order preserved", failures);
    expect(result.records[2].request.type == full_engine::WorldChunkResidencyRequestType::MakeResident, "third request order preserved", failures);
    expect(result.records[0].finalState == full_engine::ChunkResidencyState::Resident, "first final state resident", failures);
    expect(result.records[1].finalState == full_engine::ChunkResidencyState::Unloaded, "second final state unloaded", failures);
    expect(result.records[2].finalState == full_engine::ChunkResidencyState::Resident, "third final state resident", failures);
    expectResidency(registry, id, full_engine::ChunkResidencyState::Resident, "repeated requests end resident", failures);
}

void testClearAndEmptyApply(std::vector<std::string>& failures)
{
    full_engine::WorldChunkRegistry registry;
    full_engine::WorldChunkResidencyRequestQueue queue;
    const full_engine::WorldChunkResidencyApplyResult emptyResult = queue.applyTo(registry);
    expect(emptyResult.records.empty(), "empty apply emits no records", failures);
    expect(emptyResult.summary.appliedCount == 0, "empty apply has zero applied", failures);
    expect(emptyResult.summary.notFoundCount == 0, "empty apply has zero missing", failures);

    queue.push({7, 0, 0}, full_engine::WorldChunkResidencyRequestType::MakeResident);
    expect(queue.requestCount() == 1, "push increments request count", failures);
    expect(queue.requests().size() == 1, "requests exposes queued request", failures);
    queue.clear();
    expect(queue.requestCount() == 0, "clear removes queued requests", failures);
    expect(queue.requests().empty(), "requests is empty after clear", failures);
}
} // namespace

int main()
{
    std::vector<std::string> failures;

    testMakeResident(failures);
    testMakeUnloaded(failures);
    testAlreadySatisfied(failures);
    testMissingDoesNotStopLaterRequests(failures);
    testFifoAndRepeatedRequests(failures);
    testClearAndEmptyApply(failures);

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
