#pragma once

#include "full_renderer/Renderer.hpp"

#include <cstdint>

namespace full_renderer::scene
{
struct Frustum;

struct ParticleRenderBatch
{
    ParticleBatchDesc desc;
    Aabb bounds;
    std::uint32_t firstParticle = 0;
    std::uint32_t particleCount = 0;
    std::uint32_t sourceIndex = 0;
    float distanceToCameraSq = 0.0f;
    bool usesFallbackTexture = false;
};

struct ParticleRenderPlanInput
{
    const Frustum* cameraFrustum = nullptr;
    const float* cameraPositionWorld = nullptr;
};

struct ParticleRenderPlan
{
    ParticleRenderBatch batches[kMaxFrameParticleBatches];
    Particle particles[kMaxFrameParticles];
    std::uint32_t submittedBatchCount = 0;
    std::uint32_t acceptedBatchCount = 0;
    std::uint32_t rejectedBatchCount = 0;
    std::uint32_t culledBatchCount = 0;
    std::uint32_t submittedParticleCount = 0;
    std::uint32_t acceptedParticleCount = 0;
    std::uint32_t rejectedParticleCount = 0;
    std::uint32_t culledParticleCount = 0;
    std::uint32_t sortedBatchCount = 0;
    std::uint32_t sortedParticleCount = 0;
    std::uint32_t fallbackTextureBatchCount = 0;
    std::uint32_t drawCallCount = 0;
    float softParticleFadeDistanceMeters = 0.0f;
    bool enabled = false;
    bool frustumCullingEnabled = false;
    bool softParticlesEnabled = false;
};

bool isValidParticle(const Particle& particle) noexcept;
bool isValidParticleBatchDesc(const ParticleBatchDesc& desc) noexcept;
bool isValidParticleSubmitDesc(const ParticleSubmitDesc& desc) noexcept;
float clampSoftParticleFadeDistanceMeters(float distanceMeters) noexcept;
bool buildParticleBatchBounds(
    const Particle* particles,
    std::uint32_t particleCount,
    Aabb& outBounds) noexcept;
ParticleRenderPlan buildParticleRenderPlan(const ParticleSubmitDesc* desc) noexcept;
ParticleRenderPlan buildParticleRenderPlan(
    const ParticleSubmitDesc* desc,
    const ParticleRenderPlanInput& input) noexcept;
} // namespace full_renderer::scene
