#include "full_renderer/Animation.hpp"
#include "renderer/animation/AnimationSystem.hpp"

#include <cmath>
#include <cstdint>
#include <iostream>
#include <limits>

namespace
{
void identity(float out[16]) noexcept
{
    for (int index = 0; index < 16; ++index)
    {
        out[index] = 0.0f;
    }
    out[0] = 1.0f;
    out[5] = 1.0f;
    out[10] = 1.0f;
    out[15] = 1.0f;
}

void expect(const bool condition, const char* message, int& failures)
{
    if (!condition)
    {
        std::cerr << "FAILED: " << message << '\n';
        ++failures;
    }
}

full_renderer::SkeletonDesc makeSkeleton(full_renderer::SkeletonJointDesc joints[2])
{
    identity(joints[0].inverseBindPose);
    joints[0].parentIndex = -1;
    identity(joints[1].inverseBindPose);
    joints[1].parentIndex = 0;
    full_renderer::SkeletonDesc desc;
    desc.joints = joints;
    desc.jointCount = 2;
    return desc;
}

full_renderer::SkinnedMeshDesc makeSkinnedMesh(
    const full_renderer::SkeletonHandle skeleton,
    full_renderer::SkinnedMeshVertex vertices[3],
    std::uint16_t indices[3])
{
    for (std::uint32_t index = 0; index < 3; ++index)
    {
        vertices[index].position[0] = static_cast<float>(index);
        vertices[index].position[1] = 0.0f;
        vertices[index].position[2] = 0.0f;
        vertices[index].jointIndices[0] = 0.0f;
        vertices[index].jointIndices[1] = 1.0f;
        vertices[index].jointIndices[2] = 0.0f;
        vertices[index].jointIndices[3] = 0.0f;
        vertices[index].jointWeights[0] = 0.5f;
        vertices[index].jointWeights[1] = 0.5f;
        vertices[index].jointWeights[2] = 0.0f;
        vertices[index].jointWeights[3] = 0.0f;
    }
    indices[0] = 0;
    indices[1] = 1;
    indices[2] = 2;

    full_renderer::SkinnedMeshDesc desc;
    desc.skeleton = skeleton;
    desc.vertices = vertices;
    desc.vertexCount = 3;
    desc.indices = indices;
    desc.indexCount = 3;
    return desc;
}

void skeletonValidation(int& failures)
{
    full_renderer::SkeletonJointDesc joints[2] = {};
    const full_renderer::SkeletonDesc valid = makeSkeleton(joints);
    expect(
        full_renderer::animation::validateSkeletonDescriptorForTests(valid),
        "valid two-joint skeleton is accepted",
        failures);

    full_renderer::SkeletonDesc missingJoints = valid;
    missingJoints.joints = nullptr;
    expect(
        !full_renderer::animation::validateSkeletonDescriptorForTests(missingJoints),
        "null skeleton joint pointer is rejected",
        failures);

    full_renderer::SkeletonJointDesc badParentJoints[2] = {};
    full_renderer::SkeletonDesc badParent = makeSkeleton(badParentJoints);
    badParentJoints[1].parentIndex = 1;
    expect(
        !full_renderer::animation::validateSkeletonDescriptorForTests(badParent),
        "non-parent-before-child skeleton is rejected",
        failures);

    full_renderer::SkeletonJointDesc badRoots[2] = {};
    full_renderer::SkeletonDesc twoRoots = makeSkeleton(badRoots);
    badRoots[1].parentIndex = -1;
    expect(
        !full_renderer::animation::validateSkeletonDescriptorForTests(twoRoots),
        "multiple skeleton roots are rejected",
        failures);
}

void paletteValidation(int& failures)
{
    float matrices[2][16] = {};
    identity(matrices[0]);
    identity(matrices[1]);
    full_renderer::SkinningPaletteDesc palette;
    palette.skinningMatrices = &matrices[0][0];
    palette.matrixCount = 2;
    expect(
        full_renderer::animation::validateSkinningPaletteForTests(palette, 2),
        "matching palette is accepted",
        failures);

    palette.matrixCount = 1;
    expect(
        !full_renderer::animation::validateSkinningPaletteForTests(palette, 2),
        "short palette is rejected",
        failures);

    palette.matrixCount = 2;
    matrices[1][0] = NAN;
    expect(
        !full_renderer::animation::validateSkinningPaletteForTests(palette, 2),
        "non-finite palette matrix is rejected",
        failures);
}

void meshAndDrawValidation(int& failures)
{
    full_renderer::animation::AnimationSystem system;
    full_renderer::SkeletonJointDesc joints[2] = {};
    const full_renderer::SkeletonHandle skeleton = system.createSkeleton(makeSkeleton(joints));
    expect(full_renderer::isValid(skeleton), "skeleton handle is created", failures);
    expect(system.liveSkeletonCount() == 1, "live skeleton count updates", failures);

    full_renderer::SkinnedMeshVertex vertices[3] = {};
    std::uint16_t indices[3] = {};
    full_renderer::SkinnedMeshDesc meshDesc = makeSkinnedMesh(skeleton, vertices, indices);
    expect(system.validateSkinnedMeshDesc(meshDesc), "valid skinned mesh is accepted", failures);

    full_renderer::SkinnedMeshSectionDesc sections[1] = {{0, 3}};
    meshDesc.sections = sections;
    meshDesc.sectionCount = 1;
    expect(system.validateSkinnedMeshDesc(meshDesc), "valid sectioned skinned mesh is accepted", failures);

    vertices[0].jointIndices[0] = 7.0f;
    expect(!system.validateSkinnedMeshDesc(meshDesc), "out-of-range joint index is rejected", failures);
    vertices[0].jointIndices[0] = 0.0f;
    vertices[0].uv0[0] = std::numeric_limits<float>::infinity();
    expect(!system.validateSkinnedMeshDesc(meshDesc), "non-finite skinned UV0 is rejected", failures);
    vertices[0].uv0[0] = 0.0f;
    vertices[0].jointWeights[0] = 0.2f;
    vertices[0].jointWeights[1] = 0.2f;
    expect(!system.validateSkinnedMeshDesc(meshDesc), "bad weight sum is rejected", failures);
    vertices[0].jointWeights[0] = 0.5f;
    vertices[0].jointWeights[1] = 0.5f;

    const full_renderer::SkinnedMeshHandle mesh{9};
    system.registerSkinnedMesh(mesh, meshDesc);
    expect(system.liveSkinnedMeshCount() == 1, "live skinned mesh count updates", failures);

    float model[16] = {};
    float paletteMatrices[2][16] = {};
    identity(model);
    identity(paletteMatrices[0]);
    identity(paletteMatrices[1]);
    full_renderer::AnimatedDrawItem draw;
    draw.mesh = mesh;
    draw.material = full_renderer::MaterialHandle{3};
    for (int index = 0; index < 16; ++index)
    {
        draw.model[index] = model[index];
    }
    draw.bounds.min[0] = -1.0f;
    draw.bounds.min[1] = -1.0f;
    draw.bounds.min[2] = -1.0f;
    draw.bounds.max[0] = 1.0f;
    draw.bounds.max[1] = 1.0f;
    draw.bounds.max[2] = 1.0f;
    draw.palette.skinningMatrices = &paletteMatrices[0][0];
    draw.palette.matrixCount = 2;
    expect(system.validateAnimatedDraws(&draw, 1), "valid animated draw is accepted", failures);

    draw.sectionIndex = 1;
    expect(!system.validateAnimatedDraws(&draw, 1), "animated draw rejects out-of-range section", failures);
    draw.sectionIndex = 0;

    draw.palette.matrixCount = 1;
    expect(!system.validateAnimatedDraws(&draw, 1), "animated draw rejects short palette", failures);
}
} // namespace

int main()
{
    int failures = 0;
    skeletonValidation(failures);
    paletteValidation(failures);
    meshAndDrawValidation(failures);
    return failures == 0 ? 0 : 1;
}
