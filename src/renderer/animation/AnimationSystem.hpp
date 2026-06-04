#pragma once

#include "full_renderer/Animation.hpp"
#include "renderer/debug/TerrainDebug.hpp"

#include <cstdint>
#include <vector>

namespace full_renderer::animation
{
struct SkinnedMeshMetadata
{
    SkinnedMeshHandle handle;
    SkeletonHandle skeleton;
    std::uint32_t jointCount = 0;
    std::uint32_t sectionCount = 0;
    bool active = false;
};

class AnimationSystem
{
public:
    static bool validateSkeletonDesc(const SkeletonDesc& desc) noexcept;
    static bool validateSkinningPalette(const SkinningPaletteDesc& palette, std::uint32_t jointCount) noexcept;
    SkeletonHandle createSkeleton(const SkeletonDesc& desc);
    void destroySkeleton(SkeletonHandle handle) noexcept;
    bool validateSkinnedMeshDesc(const SkinnedMeshDesc& desc) const noexcept;
    void registerSkinnedMesh(SkinnedMeshHandle handle, const SkinnedMeshDesc& desc);
    void unregisterSkinnedMesh(SkinnedMeshHandle handle) noexcept;
    bool validateAnimatedDraws(const AnimatedDrawItem* draws, std::uint32_t drawCount) const noexcept;
    std::uint32_t appendDebugLines(
        const AnimatedDrawItem* draws,
        std::uint32_t drawCount,
        const AnimationDebugOptions& options,
        std::vector<debug::DebugLineVertex>& outLines) const;
    void clear() noexcept;
    std::uint32_t liveSkeletonCount() const noexcept;
    std::uint32_t liveSkinnedMeshCount() const noexcept;

private:
    struct SkeletonRecord
    {
        SkeletonHandle handle;
        std::vector<SkeletonJointDesc> joints;
        bool active = false;
    };

    const SkeletonRecord* findSkeleton(SkeletonHandle handle) const noexcept;
    const SkinnedMeshMetadata* findSkinnedMesh(SkinnedMeshHandle handle) const noexcept;

    std::vector<SkeletonRecord> skeletons_;
    std::vector<SkinnedMeshMetadata> skinnedMeshes_;
    std::uint32_t nextSkeletonId_ = 1;
};

bool validateSkeletonDescriptorForTests(const SkeletonDesc& desc) noexcept;
bool validateSkinningPaletteForTests(const SkinningPaletteDesc& palette, std::uint32_t jointCount) noexcept;
} // namespace full_renderer::animation
