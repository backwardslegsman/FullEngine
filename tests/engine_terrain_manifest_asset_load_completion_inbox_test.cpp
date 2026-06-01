#include "engine/renderer_integration/TerrainManifestAssetLoadCompletionAdapter.hpp"

#include <cstdlib>
#include <cstdint>
#include <iostream>
#include <string>
#include <vector>

namespace
{
void expect(const bool condition, const char* const message, std::vector<std::string>& failures)
{
    if (!condition)
    {
        failures.emplace_back(message);
    }
}

full_engine::AssetId asset(const std::uint64_t value) noexcept
{
    return {value};
}

full_engine::TerrainManifestAssetLoadJobCompletion completion(
    const std::uint64_t id,
    const full_engine::AssetKind kind,
    const std::uint32_t handleValue)
{
    full_engine::TerrainManifestAssetLoadJobCompletion result;
    result.request.id = asset(id);
    result.request.kind = kind;
    result.output.status = full_engine::TerrainManifestAssetLoadCallbackStatus::Loaded;
    switch (kind)
    {
    case full_engine::AssetKind::Mesh:
        result.output.mesh = {handleValue};
        break;
    case full_engine::AssetKind::Material:
        result.output.material = {handleValue};
        break;
    case full_engine::AssetKind::Texture:
        result.output.texture = {handleValue};
        break;
    case full_engine::AssetKind::Unknown:
    case full_engine::AssetKind::TerrainChunk:
    case full_engine::AssetKind::Skeleton:
    case full_engine::AssetKind::SkinnedMesh:
    case full_engine::AssetKind::Shader:
        break;
    }
    return result;
}

void testDefaultInbox(std::vector<std::string>& failures)
{
    const full_engine::TerrainManifestAssetLoadCompletionInbox inbox;
    expect(inbox.completionCount() == 0, "default inbox is empty", failures);
    expect(inbox.completions().empty(), "default inbox has no completion records", failures);
    expect(!inbox.contains(asset(1), full_engine::AssetKind::Mesh), "default inbox contains no mesh", failures);
}

void testValidCompletionsPublishInOrder(std::vector<std::string>& failures)
{
    full_engine::TerrainManifestAssetLoadCompletionInbox inbox;
    const std::vector<full_engine::TerrainManifestAssetLoadJobCompletion> completions = {
        completion(3, full_engine::AssetKind::Texture, 30),
        completion(1, full_engine::AssetKind::Mesh, 10),
        completion(2, full_engine::AssetKind::Material, 20),
    };

    const full_engine::TerrainManifestAssetLoadCompletionInboxPublishResult result =
        inbox.publish(completions.data(), completions.size());

    expect(result.records.size() == 3, "valid publish returns one record per completion", failures);
    expect(result.summary.publishedCount == 3, "valid publish counts three completions", failures);
    expect(inbox.completionCount() == 3, "valid publish retains three completions", failures);
    expect(inbox.completions()[0].request.id == asset(3), "valid publish preserves first completion order", failures);
    expect(inbox.completions()[1].request.id == asset(1), "valid publish preserves second completion order", failures);
    expect(inbox.completions()[2].request.id == asset(2), "valid publish preserves third completion order", failures);
    expect(inbox.contains(asset(1), full_engine::AssetKind::Mesh), "valid publish contains mesh", failures);
    expect(inbox.contains(asset(2), full_engine::AssetKind::Material), "valid publish contains material", failures);
    expect(inbox.contains(asset(3), full_engine::AssetKind::Texture), "valid publish contains texture", failures);
}

void testDuplicatesPreserveOneCompletion(std::vector<std::string>& failures)
{
    full_engine::TerrainManifestAssetLoadCompletionInbox inbox;
    const full_engine::TerrainManifestAssetLoadJobCompletion first =
        completion(1, full_engine::AssetKind::Mesh, 10);
    const full_engine::TerrainManifestAssetLoadJobCompletion duplicate =
        completion(1, full_engine::AssetKind::Mesh, 11);

    const full_engine::TerrainManifestAssetLoadCompletionInboxRecord firstRecord =
        inbox.publish(first);
    const full_engine::TerrainManifestAssetLoadCompletionInboxRecord duplicateRecord =
        inbox.publish(duplicate);

    expect(firstRecord.status == full_engine::TerrainManifestAssetLoadCompletionInboxStatus::Published, "first duplicate test completion publishes", failures);
    expect(duplicateRecord.status == full_engine::TerrainManifestAssetLoadCompletionInboxStatus::AlreadyPublished, "duplicate completion reports already published", failures);
    expect(inbox.completionCount() == 1, "duplicate completion preserves one retained record", failures);
    expect(inbox.completions()[0].output.mesh.id == 10, "duplicate completion preserves first handle", failures);
}

void testInvalidCompletionsAreRejected(std::vector<std::string>& failures)
{
    full_engine::TerrainManifestAssetLoadCompletionInbox inbox;
    full_engine::TerrainManifestAssetLoadJobCompletion invalidId =
        completion(0, full_engine::AssetKind::Mesh, 10);
    full_engine::TerrainManifestAssetLoadJobCompletion unsupportedKind =
        completion(4, full_engine::AssetKind::TerrainChunk, 40);
    full_engine::TerrainManifestAssetLoadJobCompletion missingStatus =
        completion(2, full_engine::AssetKind::Material, 20);
    missingStatus.output.status = full_engine::TerrainManifestAssetLoadCallbackStatus::Missing;
    full_engine::TerrainManifestAssetLoadJobCompletion failedStatus =
        completion(2, full_engine::AssetKind::Material, 20);
    failedStatus.output.status = full_engine::TerrainManifestAssetLoadCallbackStatus::Failed;
    full_engine::TerrainManifestAssetLoadJobCompletion defaultHandle =
        completion(3, full_engine::AssetKind::Texture, 0);
    const std::vector<full_engine::TerrainManifestAssetLoadJobCompletion> completions = {
        invalidId,
        unsupportedKind,
        missingStatus,
        failedStatus,
        defaultHandle,
    };

    const full_engine::TerrainManifestAssetLoadCompletionInboxPublishResult result =
        inbox.publish(completions.data(), completions.size());

    expect(result.summary.invalidRequestCount == 2, "invalid publish counts bad request identity/kind", failures);
    expect(result.summary.missingHandleCount == 3, "invalid publish counts missing handles/statuses", failures);
    expect(inbox.completionCount() == 0, "invalid publish does not mutate inbox", failures);
}

void testNullBatchAndClear(std::vector<std::string>& failures)
{
    full_engine::TerrainManifestAssetLoadCompletionInbox inbox;
    (void)inbox.publish(completion(1, full_engine::AssetKind::Mesh, 10));

    const full_engine::TerrainManifestAssetLoadCompletionInboxPublishResult invalid =
        inbox.publish(nullptr, 2);
    expect(invalid.summary.invalidRequestCount == 2, "null non-empty publish reports invalid requests", failures);
    expect(inbox.completionCount() == 1, "null non-empty publish preserves existing completion", failures);

    inbox.clear();
    expect(inbox.completionCount() == 0, "clear removes completions", failures);
    expect(inbox.completions().empty(), "clear empties completion vector", failures);
}

void testRemoveCompletion(std::vector<std::string>& failures)
{
    full_engine::TerrainManifestAssetLoadCompletionInbox inbox;
    (void)inbox.publish(completion(1, full_engine::AssetKind::Mesh, 10));
    (void)inbox.publish(completion(2, full_engine::AssetKind::Material, 20));
    (void)inbox.publish(completion(3, full_engine::AssetKind::Texture, 30));

    const full_engine::TerrainManifestAssetLoadCompletionInboxRemoveResult invalid =
        inbox.remove(asset(0), full_engine::AssetKind::Mesh);
    const full_engine::TerrainManifestAssetLoadCompletionInboxRemoveResult missing =
        inbox.remove(asset(4), full_engine::AssetKind::Texture);
    const full_engine::TerrainManifestAssetLoadCompletionInboxRemoveResult removed =
        inbox.remove(asset(2), full_engine::AssetKind::Material);

    expect(invalid.status == full_engine::TerrainManifestAssetLoadCompletionInboxRemoveStatus::InvalidArgument, "remove rejects invalid request", failures);
    expect(missing.status == full_engine::TerrainManifestAssetLoadCompletionInboxRemoveStatus::NotFound, "remove reports missing completion", failures);
    expect(removed.status == full_engine::TerrainManifestAssetLoadCompletionInboxRemoveStatus::Removed, "remove reports removed completion", failures);
    expect(inbox.completionCount() == 2, "remove drops one retained completion", failures);
    expect(!inbox.contains(asset(2), full_engine::AssetKind::Material), "remove clears matching completion", failures);
    expect(inbox.completions()[0].request.id == asset(1), "remove preserves prior completion order", failures);
    expect(inbox.completions()[1].request.id == asset(3), "remove preserves later completion order", failures);
}

void testReplaceCompletion(std::vector<std::string>& failures)
{
    full_engine::TerrainManifestAssetLoadCompletionInbox inbox;
    (void)inbox.publish(completion(1, full_engine::AssetKind::Mesh, 10));
    (void)inbox.publish(completion(2, full_engine::AssetKind::Material, 20));

    const full_engine::TerrainManifestAssetLoadCompletionInboxReplaceResult replaced =
        inbox.replace(completion(1, full_engine::AssetKind::Mesh, 11));
    const full_engine::TerrainManifestAssetLoadCompletionInboxReplaceResult appended =
        inbox.replace(completion(3, full_engine::AssetKind::Texture, 30));

    full_engine::TerrainManifestAssetLoadJobCompletion invalid =
        completion(0, full_engine::AssetKind::Mesh, 40);
    const full_engine::TerrainManifestAssetLoadCompletionInboxReplaceResult invalidResult =
        inbox.replace(invalid);

    full_engine::TerrainManifestAssetLoadJobCompletion missing =
        completion(2, full_engine::AssetKind::Material, 0);
    const full_engine::TerrainManifestAssetLoadCompletionInboxReplaceResult missingResult =
        inbox.replace(missing);

    expect(replaced.status == full_engine::TerrainManifestAssetLoadCompletionInboxReplaceStatus::Replaced, "replace reports existing completion replacement", failures);
    expect(appended.status == full_engine::TerrainManifestAssetLoadCompletionInboxReplaceStatus::Published, "replace publishes new completion", failures);
    expect(invalidResult.status == full_engine::TerrainManifestAssetLoadCompletionInboxReplaceStatus::InvalidRequest, "replace rejects invalid request", failures);
    expect(missingResult.status == full_engine::TerrainManifestAssetLoadCompletionInboxReplaceStatus::MissingHandle, "replace rejects missing handle", failures);
    expect(inbox.completionCount() == 3, "replace retains expected completion count", failures);
    expect(inbox.completions()[0].request.id == asset(1), "replace preserves replaced completion slot", failures);
    expect(inbox.completions()[0].output.mesh.id == 11, "replace overwrites retained handle", failures);
    expect(inbox.completions()[1].request.id == asset(2), "replace preserves unaffected completion order", failures);
    expect(inbox.completions()[2].request.id == asset(3), "replace appends new completion", failures);
}

void testWorkerAdapterPublishesIntoInbox(std::vector<std::string>& failures)
{
    full_engine::TerrainManifestAssetLoadCompletionInbox inbox;
    const std::vector<full_engine::TerrainManifestAssetLoadJobCompletion> completions = {
        completion(1, full_engine::AssetKind::Mesh, 10),
        completion(2, full_engine::AssetKind::Material, 20),
    };

    const full_engine::TerrainManifestAssetLoadWorkerCompletionPublishResult first =
        full_engine::publishTerrainManifestAssetLoadWorkerCompletions(
            inbox,
            completions.data(),
            completions.size());
    const full_engine::TerrainManifestAssetLoadWorkerCompletionPublishResult duplicate =
        full_engine::publishTerrainManifestAssetLoadWorkerCompletion(
            inbox,
            completions[0]);

    expect(first.publish.summary.publishedCount == 2, "worker adapter publishes batch completions", failures);
    expect(first.pendingCompletionCount == 2, "worker adapter reports pending completion count", failures);
    expect(duplicate.publish.summary.alreadyPublishedCount == 1, "worker adapter reports duplicate completion", failures);
    expect(duplicate.pendingCompletionCount == 2, "worker adapter preserves pending count after duplicate", failures);
    expect(inbox.completionCount() == 2, "worker adapter retains completions in inbox", failures);
}

void testWorkerAdapterReplaceAndRemove(std::vector<std::string>& failures)
{
    full_engine::TerrainManifestAssetLoadCompletionInbox inbox;
    (void)full_engine::publishTerrainManifestAssetLoadWorkerCompletion(
        inbox,
        completion(1, full_engine::AssetKind::Mesh, 10));
    (void)full_engine::publishTerrainManifestAssetLoadWorkerCompletion(
        inbox,
        completion(2, full_engine::AssetKind::Texture, 20));

    const full_engine::TerrainManifestAssetLoadWorkerCompletionReplaceResult replaced =
        full_engine::replaceTerrainManifestAssetLoadWorkerCompletion(
            inbox,
            completion(1, full_engine::AssetKind::Mesh, 11));
    const full_engine::TerrainManifestAssetLoadWorkerCompletionRemoveResult removed =
        full_engine::removeTerrainManifestAssetLoadWorkerCompletion(
            inbox,
            asset(2),
            full_engine::AssetKind::Texture);

    expect(replaced.replace.status == full_engine::TerrainManifestAssetLoadCompletionInboxReplaceStatus::Replaced, "worker adapter reports replacement", failures);
    expect(replaced.pendingCompletionCount == 2, "worker adapter replace preserves pending count", failures);
    expect(removed.remove.status == full_engine::TerrainManifestAssetLoadCompletionInboxRemoveStatus::Removed, "worker adapter reports removal", failures);
    expect(removed.pendingCompletionCount == 1, "worker adapter remove reports pending count", failures);
    expect(inbox.completionCount() == 1, "worker adapter remove mutates inbox", failures);
    expect(inbox.completions()[0].output.mesh.id == 11, "worker adapter replace updates retained completion", failures);
}
} // namespace

int main()
{
    std::vector<std::string> failures;
    testDefaultInbox(failures);
    testValidCompletionsPublishInOrder(failures);
    testDuplicatesPreserveOneCompletion(failures);
    testInvalidCompletionsAreRejected(failures);
    testNullBatchAndClear(failures);
    testRemoveCompletion(failures);
    testReplaceCompletion(failures);
    testWorkerAdapterPublishesIntoInbox(failures);
    testWorkerAdapterReplaceAndRemove(failures);

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
