#include "engine/renderer_integration/TerrainManifestAssetLoadPlan.hpp"

#include <cassert>
#include <cstdlib>

namespace
{
full_engine::AssetId asset(const std::uint64_t value) noexcept
{
    return {value};
}
} // namespace

int main()
{
    {
        const full_engine::TerrainManifestAssetLoadRequestPlan plan =
            full_engine::buildTerrainManifestAssetLoadRequestPlan({});
        assert(plan.requests.empty());
        assert(plan.summary.requestCount == 0);
        assert(plan.summary.meshRequestCount == 0);
        assert(plan.summary.materialRequestCount == 0);
        assert(plan.summary.textureRequestCount == 0);
    }

    {
        full_engine::TerrainManifestAssetReadinessPlan readiness;
        readiness.records.push_back({
            asset(1),
            full_engine::TerrainManifestAssetHandleKind::Mesh,
            full_engine::TerrainManifestAssetReadinessStatus::Ready,
        });
        readiness.records.push_back({
            asset(2),
            full_engine::TerrainManifestAssetHandleKind::Mesh,
            full_engine::TerrainManifestAssetReadinessStatus::MissingHandle,
        });
        readiness.records.push_back({
            asset(3),
            full_engine::TerrainManifestAssetHandleKind::Material,
            full_engine::TerrainManifestAssetReadinessStatus::MissingHandle,
        });
        readiness.records.push_back({
            asset(4),
            full_engine::TerrainManifestAssetHandleKind::Texture,
            full_engine::TerrainManifestAssetReadinessStatus::MissingHandle,
        });
        readiness.summary.missingHandleCount = 3;

        const full_engine::TerrainManifestAssetLoadRequestPlan plan =
            full_engine::buildTerrainManifestAssetLoadRequestPlan(readiness);

        assert(plan.requests.size() == 3);
        assert(plan.summary.requestCount == 3);
        assert(plan.summary.meshRequestCount == 1);
        assert(plan.summary.materialRequestCount == 1);
        assert(plan.summary.textureRequestCount == 1);
        assert(plan.requests[0].id == asset(2));
        assert(plan.requests[0].kind == full_engine::AssetKind::Mesh);
        assert(plan.requests[1].id == asset(3));
        assert(plan.requests[1].kind == full_engine::AssetKind::Material);
        assert(plan.requests[2].id == asset(4));
        assert(plan.requests[2].kind == full_engine::AssetKind::Texture);
        assert(readiness.records.size() == 4);
    }

    return EXIT_SUCCESS;
}
