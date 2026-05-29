#include "renderer/scene/Particle.hpp"

#include "renderer/scene/Frustum.hpp"
#include "renderer/scene/Math.hpp"

#include <algorithm>
#include <cmath>

namespace full_renderer::scene
{
namespace
{
constexpr float kMinimumParticleSizeMeters = 0.001f;
constexpr float kMaximumSoftParticleFadeDistanceMeters = 100.0f;

bool isUnitRangeColor4(const float values[4]) noexcept
{
    for (int index = 0; index < 4; ++index)
    {
        if (!std::isfinite(values[index]) || values[index] < 0.0f || values[index] > 1.0f)
        {
            return false;
        }
    }

    return true;
}

bool isValidUvRect(const float values[4]) noexcept
{
    for (int index = 0; index < 4; ++index)
    {
        if (!std::isfinite(values[index]) || values[index] < 0.0f || values[index] > 1.0f)
        {
            return false;
        }
    }

    return values[2] > values[0] && values[3] > values[1];
}

bool isValidBlendMode(const ParticleBlendMode mode) noexcept
{
    switch (mode)
    {
    case ParticleBlendMode::Alpha:
        return true;
    }

    return false;
}

bool isValidSortMode(const ParticleSortMode mode) noexcept
{
    switch (mode)
    {
    case ParticleSortMode::SubmissionOrder:
    case ParticleSortMode::BatchDistanceBackToFront:
    case ParticleSortMode::ParticleDistanceBackToFront:
        return true;
    }

    return false;
}

float distanceSqToCamera(const Aabb& bounds, const float cameraPositionWorld[3]) noexcept
{
    const Vec3 center = aabbCenter(bounds);
    const float x = center.x - cameraPositionWorld[0];
    const float y = center.y - cameraPositionWorld[1];
    const float z = center.z - cameraPositionWorld[2];
    return x * x + y * y + z * z;
}

float distanceSqToCamera(const Particle& particle, const float cameraPositionWorld[3]) noexcept
{
    const float x = particle.positionWorld[0] - cameraPositionWorld[0];
    const float y = particle.positionWorld[1] - cameraPositionWorld[1];
    const float z = particle.positionWorld[2] - cameraPositionWorld[2];
    return x * x + y * y + z * z;
}

void sortParticlesBackToFront(
    Particle* particles,
    const std::uint32_t count,
    const float cameraPositionWorld[3]) noexcept
{
    for (std::uint32_t index = 1; index < count; ++index)
    {
        Particle item = particles[index];
        const float itemDistance = distanceSqToCamera(item, cameraPositionWorld);
        std::uint32_t insertAt = index;
        while (insertAt > 0)
        {
            const float previousDistance = distanceSqToCamera(particles[insertAt - 1U], cameraPositionWorld);
            if (previousDistance >= itemDistance)
            {
                break;
            }
            particles[insertAt] = particles[insertAt - 1U];
            --insertAt;
        }
        particles[insertAt] = item;
    }
}

void sortBatchesBackToFront(ParticleRenderPlan& plan) noexcept
{
    for (std::uint32_t index = 1; index < plan.acceptedBatchCount; ++index)
    {
        ParticleRenderBatch item = plan.batches[index];
        std::uint32_t insertAt = index;
        while (insertAt > 0)
        {
            const ParticleRenderBatch& previous = plan.batches[insertAt - 1U];
            if (previous.distanceToCameraSq > item.distanceToCameraSq ||
                (previous.distanceToCameraSq == item.distanceToCameraSq && previous.sourceIndex <= item.sourceIndex))
            {
                break;
            }
            plan.batches[insertAt] = previous;
            --insertAt;
        }
        plan.batches[insertAt] = item;
    }
}
} // namespace

bool isValidParticle(const Particle& particle) noexcept
{
    return isFinite3(particle.positionWorld) &&
        std::isfinite(particle.sizeMeters) &&
        particle.sizeMeters >= kMinimumParticleSizeMeters &&
        isUnitRangeColor4(particle.colorLinear) &&
        std::isfinite(particle.rotationRadians) &&
        isValidUvRect(particle.uvRect);
}

bool isValidParticleBatchDesc(const ParticleBatchDesc& desc) noexcept
{
    if (!isValidBlendMode(desc.blendMode) || !isValidSortMode(desc.sortMode))
    {
        return false;
    }

    return desc.particleCount == 0 || desc.particles != nullptr;
}

bool isValidParticleSubmitDesc(const ParticleSubmitDesc& desc) noexcept
{
    if (!desc.enabled)
    {
        return true;
    }

    if (!isValidSortMode(desc.sortMode) ||
        !std::isfinite(desc.softParticleFadeDistanceMeters))
    {
        return false;
    }

    return desc.batchCount == 0 || desc.batches != nullptr;
}

float clampSoftParticleFadeDistanceMeters(const float distanceMeters) noexcept
{
    if (!std::isfinite(distanceMeters) || distanceMeters <= 0.0f)
    {
        return 0.0f;
    }

    return std::min(distanceMeters, kMaximumSoftParticleFadeDistanceMeters);
}

bool buildParticleBatchBounds(
    const Particle* particles,
    const std::uint32_t particleCount,
    Aabb& outBounds) noexcept
{
    bool hasParticle = false;
    for (std::uint32_t particleIndex = 0; particleIndex < particleCount; ++particleIndex)
    {
        const Particle& particle = particles[particleIndex];
        if (!isValidParticle(particle))
        {
            continue;
        }

        const float radius = particle.sizeMeters * 0.5f;
        const float minValues[3] = {
            particle.positionWorld[0] - radius,
            particle.positionWorld[1] - radius,
            particle.positionWorld[2] - radius};
        const float maxValues[3] = {
            particle.positionWorld[0] + radius,
            particle.positionWorld[1] + radius,
            particle.positionWorld[2] + radius};
        if (!hasParticle)
        {
            for (int axis = 0; axis < 3; ++axis)
            {
                outBounds.min[axis] = minValues[axis];
                outBounds.max[axis] = maxValues[axis];
            }
            hasParticle = true;
            continue;
        }

        for (int axis = 0; axis < 3; ++axis)
        {
            outBounds.min[axis] = std::min(outBounds.min[axis], minValues[axis]);
            outBounds.max[axis] = std::max(outBounds.max[axis], maxValues[axis]);
        }
    }

    return hasParticle && isValidAabb(outBounds);
}

ParticleRenderPlan buildParticleRenderPlan(const ParticleSubmitDesc* desc) noexcept
{
    return buildParticleRenderPlan(desc, {});
}

ParticleRenderPlan buildParticleRenderPlan(
    const ParticleSubmitDesc* desc,
    const ParticleRenderPlanInput& input) noexcept
{
    ParticleRenderPlan plan;
    if (desc == nullptr || !desc->enabled)
    {
        return plan;
    }

    plan.enabled = true;
    plan.frustumCullingEnabled = desc->cullAgainstViewFrustum && input.cameraFrustum != nullptr;
    plan.softParticlesEnabled = desc->softParticlesEnabled;
    plan.softParticleFadeDistanceMeters =
        desc->softParticlesEnabled ? clampSoftParticleFadeDistanceMeters(desc->softParticleFadeDistanceMeters) : 0.0f;
    plan.submittedBatchCount = desc->batchCount;
    if (desc->batchCount > 0 && desc->batches == nullptr)
    {
        plan.rejectedBatchCount = desc->batchCount;
        return plan;
    }

    const std::uint32_t batchCount = std::min(desc->batchCount, kMaxFrameParticleBatches);
    plan.rejectedBatchCount = desc->batchCount - batchCount;
    for (std::uint32_t batchIndex = 0; batchIndex < batchCount; ++batchIndex)
    {
        const ParticleBatchDesc& sourceBatch = desc->batches[batchIndex];
        plan.submittedParticleCount += sourceBatch.particleCount;
        if (!isValidParticleBatchDesc(sourceBatch))
        {
            ++plan.rejectedBatchCount;
            plan.rejectedParticleCount += sourceBatch.particleCount;
            continue;
        }

        ParticleRenderBatch batch;
        batch.desc = sourceBatch;
        batch.desc.particles = nullptr;
        batch.sourceIndex = batchIndex;
        batch.firstParticle = plan.acceptedParticleCount;
        batch.usesFallbackTexture = !isValid(sourceBatch.texture);

        for (std::uint32_t particleIndex = 0; particleIndex < sourceBatch.particleCount; ++particleIndex)
        {
            if (plan.acceptedParticleCount >= kMaxFrameParticles)
            {
                ++plan.rejectedParticleCount;
                continue;
            }

            const Particle& particle = sourceBatch.particles[particleIndex];
            if (!isValidParticle(particle))
            {
                ++plan.rejectedParticleCount;
                continue;
            }

            plan.particles[plan.acceptedParticleCount] = particle;
            ++plan.acceptedParticleCount;
            ++batch.particleCount;
        }

        if (batch.particleCount == 0)
        {
            if (sourceBatch.particleCount > 0)
            {
                ++plan.rejectedBatchCount;
            }
            continue;
        }

        if (!buildParticleBatchBounds(plan.particles + batch.firstParticle, batch.particleCount, batch.bounds))
        {
            ++plan.rejectedBatchCount;
            plan.rejectedParticleCount += batch.particleCount;
            plan.acceptedParticleCount = batch.firstParticle;
            continue;
        }

        if (plan.frustumCullingEnabled && !intersects(*input.cameraFrustum, batch.bounds))
        {
            ++plan.culledBatchCount;
            plan.culledParticleCount += batch.particleCount;
            plan.acceptedParticleCount = batch.firstParticle;
            continue;
        }

        if (input.cameraPositionWorld != nullptr)
        {
            batch.distanceToCameraSq = distanceSqToCamera(batch.bounds, input.cameraPositionWorld);
        }

        const bool sortParticles =
            input.cameraPositionWorld != nullptr &&
            (desc->sortMode == ParticleSortMode::ParticleDistanceBackToFront ||
                sourceBatch.sortMode == ParticleSortMode::ParticleDistanceBackToFront);
        if (sortParticles && batch.particleCount > 1U)
        {
            sortParticlesBackToFront(plan.particles + batch.firstParticle, batch.particleCount, input.cameraPositionWorld);
            plan.sortedParticleCount += batch.particleCount;
        }

        batch.desc.particleCount = batch.particleCount;
        plan.batches[plan.acceptedBatchCount] = batch;
        ++plan.acceptedBatchCount;
        ++plan.drawCallCount;
        if (batch.usesFallbackTexture)
        {
            ++plan.fallbackTextureBatchCount;
        }
    }

    if (input.cameraPositionWorld != nullptr &&
        desc->sortMode == ParticleSortMode::BatchDistanceBackToFront &&
        plan.acceptedBatchCount > 1U)
    {
        sortBatchesBackToFront(plan);
        plan.sortedBatchCount = plan.acceptedBatchCount;
    }

    return plan;
}
} // namespace full_renderer::scene
