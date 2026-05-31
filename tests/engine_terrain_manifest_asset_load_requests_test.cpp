#include "engine/renderer_integration/TerrainManifestAssetLoadRequests.hpp"

#include <cassert>
#include <cstdlib>

namespace
{
full_engine::AssetId asset(const std::uint64_t value) noexcept
{
    return {value};
}

full_engine::TerrainManifestAssetLoadRequest request(
    const std::uint64_t id,
    const full_engine::AssetKind kind) noexcept
{
    return {asset(id), kind};
}
} // namespace

int main()
{
    {
        const full_engine::TerrainManifestAssetLoadRequestQueue queue;
        assert(queue.requestCount() == 0);
        assert(queue.requests().empty());
        assert(queue.summary().requestCount == 0);
        assert(!queue.contains(asset(1), full_engine::AssetKind::Mesh));
    }

    {
        full_engine::TerrainManifestAssetLoadRequestQueue queue;
        assert(queue.push(request(1, full_engine::AssetKind::Mesh)).result ==
            full_engine::TerrainManifestAssetLoadQueueResult::Queued);
        assert(queue.push(request(2, full_engine::AssetKind::Material)).result ==
            full_engine::TerrainManifestAssetLoadQueueResult::Queued);
        assert(queue.push(request(3, full_engine::AssetKind::Texture)).result ==
            full_engine::TerrainManifestAssetLoadQueueResult::Queued);
        assert(queue.requestCount() == 3);
        assert(queue.contains(asset(1), full_engine::AssetKind::Mesh));
        assert(queue.summary().meshRequestCount == 1);
        assert(queue.summary().materialRequestCount == 1);
        assert(queue.summary().textureRequestCount == 1);
    }

    {
        full_engine::TerrainManifestAssetLoadRequestQueue queue;
        assert(queue.push(request(0, full_engine::AssetKind::Mesh)).result ==
            full_engine::TerrainManifestAssetLoadQueueResult::InvalidArgument);
        assert(queue.push(request(1, full_engine::AssetKind::Skeleton)).result ==
            full_engine::TerrainManifestAssetLoadQueueResult::InvalidArgument);
        assert(queue.requestCount() == 0);
    }

    {
        full_engine::TerrainManifestAssetLoadRequestQueue queue;
        assert(queue.push(request(1, full_engine::AssetKind::Mesh)).result ==
            full_engine::TerrainManifestAssetLoadQueueResult::Queued);
        assert(queue.push(request(1, full_engine::AssetKind::Mesh)).result ==
            full_engine::TerrainManifestAssetLoadQueueResult::AlreadyQueued);
        assert(queue.push(request(1, full_engine::AssetKind::Texture)).result ==
            full_engine::TerrainManifestAssetLoadQueueResult::Queued);
        assert(queue.requestCount() == 2);
        assert(queue.requests()[0].kind == full_engine::AssetKind::Mesh);
        assert(queue.requests()[1].kind == full_engine::AssetKind::Texture);
    }

    {
        full_engine::TerrainManifestAssetLoadRequestQueue queue;
        full_engine::TerrainManifestAssetLoadRequestPlan plan;
        plan.requests.push_back(request(1, full_engine::AssetKind::Mesh));
        plan.requests.push_back(request(2, full_engine::AssetKind::Material));
        plan.requests.push_back(request(1, full_engine::AssetKind::Mesh));
        plan.requests.push_back(request(0, full_engine::AssetKind::Texture));
        plan.requests.push_back(request(3, full_engine::AssetKind::Texture));

        const full_engine::TerrainManifestAssetLoadQueuePushResult result = queue.pushPlan(plan);
        assert(result.records.size() == 5);
        assert(result.records[0].result == full_engine::TerrainManifestAssetLoadQueueResult::Queued);
        assert(result.records[1].result == full_engine::TerrainManifestAssetLoadQueueResult::Queued);
        assert(result.records[2].result == full_engine::TerrainManifestAssetLoadQueueResult::AlreadyQueued);
        assert(result.records[3].result == full_engine::TerrainManifestAssetLoadQueueResult::InvalidArgument);
        assert(result.records[4].result == full_engine::TerrainManifestAssetLoadQueueResult::Queued);
        assert(result.summary.queuedCount == 3);
        assert(result.summary.alreadyQueuedCount == 1);
        assert(result.summary.invalidArgumentCount == 1);
        assert(queue.requestCount() == 3);
        assert(queue.requests()[0].id == asset(1));
        assert(queue.requests()[1].id == asset(2));
        assert(queue.requests()[2].id == asset(3));

        queue.clear();
        assert(queue.requestCount() == 0);
        assert(queue.requests().empty());
    }

    return EXIT_SUCCESS;
}
