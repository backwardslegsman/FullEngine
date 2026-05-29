#include "renderer/scene/Particle.hpp"

#include "renderer/scene/Frustum.hpp"

#include <cstdlib>
#include <iostream>
#include <limits>

namespace
{
void expect(const bool condition, const char* message, int& failures)
{
    if (!condition)
    {
        std::cerr << "FAILED: " << message << '\n';
        ++failures;
    }
}

full_renderer::Particle makeParticle(const float x, const float y, const float z, const float size) noexcept
{
    full_renderer::Particle particle;
    particle.positionWorld[0] = x;
    particle.positionWorld[1] = y;
    particle.positionWorld[2] = z;
    particle.sizeMeters = size;
    particle.colorLinear[0] = 0.7f;
    particle.colorLinear[1] = 0.8f;
    particle.colorLinear[2] = 0.9f;
    particle.colorLinear[3] = 0.5f;
    return particle;
}

void disabledAndEmptySubmissionsAreNeutral(int& failures)
{
    const full_renderer::ParticleSubmitDesc disabled;
    expect(full_renderer::scene::isValidParticleSubmitDesc(disabled), "disabled particle submit validates", failures);

    const full_renderer::scene::ParticleRenderPlan nullPlan =
        full_renderer::scene::buildParticleRenderPlan(nullptr);
    expect(!nullPlan.enabled, "null particle submit produces disabled plan", failures);

    full_renderer::ParticleSubmitDesc empty;
    empty.enabled = true;
    const full_renderer::scene::ParticleRenderPlan emptyPlan =
        full_renderer::scene::buildParticleRenderPlan(&empty);
    expect(emptyPlan.enabled, "enabled empty particle submit produces enabled plan", failures);
    expect(emptyPlan.acceptedBatchCount == 0, "empty particle submit accepts no batches", failures);
    expect(emptyPlan.drawCallCount == 0, "empty particle submit emits no draw estimate", failures);
}

void descriptorValidationRejectsInvalidData(int& failures)
{
    full_renderer::Particle valid = makeParticle(0.0f, 1.0f, 2.0f, 0.5f);
    expect(full_renderer::scene::isValidParticle(valid), "valid particle validates", failures);

    full_renderer::Particle particle = valid;
    particle.positionWorld[0] = std::numeric_limits<float>::quiet_NaN();
    expect(!full_renderer::scene::isValidParticle(particle), "non-finite position is rejected", failures);

    particle = valid;
    particle.sizeMeters = 0.0f;
    expect(!full_renderer::scene::isValidParticle(particle), "zero particle size is rejected", failures);

    particle = valid;
    particle.colorLinear[3] = 2.0f;
    expect(!full_renderer::scene::isValidParticle(particle), "out-of-range color is rejected", failures);

    particle = valid;
    particle.rotationRadians = std::numeric_limits<float>::infinity();
    expect(!full_renderer::scene::isValidParticle(particle), "non-finite rotation is rejected", failures);

    particle = valid;
    particle.uvRect[2] = particle.uvRect[0];
    expect(!full_renderer::scene::isValidParticle(particle), "empty UV rectangle is rejected", failures);

    full_renderer::ParticleBatchDesc batch;
    batch.particles = &valid;
    batch.particleCount = 1;
    expect(full_renderer::scene::isValidParticleBatchDesc(batch), "valid particle batch validates", failures);

    batch.particles = nullptr;
    expect(!full_renderer::scene::isValidParticleBatchDesc(batch),
        "nonzero particle batch with null pointer is invalid",
        failures);

    full_renderer::ParticleSubmitDesc submit;
    submit.enabled = true;
    submit.batchCount = 1;
    expect(!full_renderer::scene::isValidParticleSubmitDesc(submit),
        "enabled particle submit with missing batch pointer is invalid",
        failures);
}

void renderPlanCopiesAndCountsAcceptedParticles(int& failures)
{
    full_renderer::Particle particles[3] = {
        makeParticle(0.0f, 0.0f, 0.0f, 0.4f),
        makeParticle(1.0f, 0.0f, 0.0f, 0.5f),
        makeParticle(2.0f, 0.0f, 0.0f, 0.6f),
    };
    particles[1].sizeMeters = -1.0f;

    full_renderer::ParticleBatchDesc batch;
    batch.particles = particles;
    batch.particleCount = 3;

    full_renderer::ParticleSubmitDesc submit;
    submit.enabled = true;
    submit.batches = &batch;
    submit.batchCount = 1;

    const full_renderer::scene::ParticleRenderPlan plan =
        full_renderer::scene::buildParticleRenderPlan(&submit);
    expect(plan.enabled, "enabled particle submit produces enabled plan", failures);
    expect(plan.submittedBatchCount == 1, "plan records submitted batch count", failures);
    expect(plan.acceptedBatchCount == 1, "plan accepts batch with at least one valid particle", failures);
    expect(plan.submittedParticleCount == 3, "plan records submitted particle count", failures);
    expect(plan.acceptedParticleCount == 2, "plan copies only valid particles", failures);
    expect(plan.rejectedParticleCount == 1, "plan tracks rejected particles", failures);
    expect(plan.fallbackTextureBatchCount == 1, "zero texture handle is counted as fallback", failures);
    expect(plan.drawCallCount == 1, "one accepted batch estimates one draw call", failures);
    expect(plan.batches[0].firstParticle == 0 && plan.batches[0].particleCount == 2,
        "accepted batch references copied particle range",
        failures);
    expect(plan.particles[1].positionWorld[0] == 2.0f,
        "particle data is copied in deterministic submission order",
        failures);
}

void batchBoundsIgnoreInvalidParticles(int& failures)
{
    full_renderer::Particle particles[3] = {
        makeParticle(-1.0f, 0.0f, 2.0f, 0.5f),
        makeParticle(3.0f, 1.0f, -2.0f, 1.0f),
        makeParticle(100.0f, 100.0f, 100.0f, 0.0f),
    };

    full_renderer::Aabb bounds;
    expect(full_renderer::scene::buildParticleBatchBounds(particles, 3, bounds),
        "particle batch bounds build from valid particles",
        failures);
    expect(bounds.min[0] == -1.25f && bounds.max[0] == 3.5f,
        "particle bounds include half-size on x",
        failures);
    expect(bounds.min[2] == -2.5f && bounds.max[2] == 2.25f,
        "particle bounds include half-size on z",
        failures);

    particles[0].sizeMeters = 0.0f;
    particles[1].sizeMeters = 0.0f;
    expect(!full_renderer::scene::buildParticleBatchBounds(particles, 3, bounds),
        "empty valid-particle bounds are rejected",
        failures);
}

void cameraFrustumCullsWholeParticleBatches(int& failures)
{
    const float identityViewProjection[16] = {
        1.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f};
    const full_renderer::scene::Frustum frustum =
        full_renderer::scene::extractFrustumFromViewProjection(identityViewProjection);
    const float cameraPosition[3] = {0.0f, 0.0f, 0.0f};
    full_renderer::scene::ParticleRenderPlanInput input;
    input.cameraFrustum = &frustum;
    input.cameraPositionWorld = cameraPosition;

    full_renderer::Particle particles[3] = {
        makeParticle(0.0f, 0.0f, 0.0f, 0.25f),
        makeParticle(3.0f, 0.0f, 0.0f, 0.25f),
        makeParticle(0.95f, 0.0f, 0.0f, 0.25f),
    };
    full_renderer::ParticleBatchDesc batches[3] = {};
    for (std::uint32_t index = 0; index < 3; ++index)
    {
        batches[index].particles = &particles[index];
        batches[index].particleCount = 1;
        batches[index].texture = full_renderer::TextureHandle{index + 1U};
    }

    full_renderer::ParticleSubmitDesc submit;
    submit.enabled = true;
    submit.batches = batches;
    submit.batchCount = 3;

    const full_renderer::scene::ParticleRenderPlan plan =
        full_renderer::scene::buildParticleRenderPlan(&submit, input);
    expect(plan.frustumCullingEnabled, "particle plan enables frustum culling with a frustum", failures);
    expect(plan.acceptedBatchCount == 2, "inside and partially intersecting batches stay active", failures);
    expect(plan.culledBatchCount == 1, "outside particle batch is culled", failures);
    expect(plan.culledParticleCount == 1, "culled batch contributes a culled particle estimate", failures);

    submit.cullAgainstViewFrustum = false;
    const full_renderer::scene::ParticleRenderPlan uncullPlan =
        full_renderer::scene::buildParticleRenderPlan(&submit, input);
    expect(!uncullPlan.frustumCullingEnabled, "particle culling can be disabled", failures);
    expect(uncullPlan.acceptedBatchCount == 3, "disabled culling preserves all accepted batches", failures);
}

void particleSortingIsDeterministicAndStable(int& failures)
{
    const float cameraPosition[3] = {0.0f, 0.0f, 0.0f};
    full_renderer::scene::ParticleRenderPlanInput input;
    input.cameraPositionWorld = cameraPosition;

    full_renderer::Particle nearParticles[1] = {makeParticle(0.0f, 0.0f, 1.0f, 0.25f)};
    full_renderer::Particle farParticles[1] = {makeParticle(0.0f, 0.0f, 5.0f, 0.25f)};
    full_renderer::ParticleBatchDesc batches[2] = {};
    batches[0].particles = nearParticles;
    batches[0].particleCount = 1;
    batches[0].texture = full_renderer::TextureHandle{1U};
    batches[1].particles = farParticles;
    batches[1].particleCount = 1;
    batches[1].texture = full_renderer::TextureHandle{2U};

    full_renderer::ParticleSubmitDesc submit;
    submit.enabled = true;
    submit.batches = batches;
    submit.batchCount = 2;
    submit.sortMode = full_renderer::ParticleSortMode::BatchDistanceBackToFront;

    const full_renderer::scene::ParticleRenderPlan sortedBatches =
        full_renderer::scene::buildParticleRenderPlan(&submit, input);
    expect(sortedBatches.sortedBatchCount == 2, "batch sorting reports sorted batches", failures);
    expect(sortedBatches.batches[0].sourceIndex == 1 && sortedBatches.batches[1].sourceIndex == 0,
        "batch sorting draws farther batch first",
        failures);

    full_renderer::Particle particles[3] = {
        makeParticle(0.0f, 0.0f, 1.0f, 0.25f),
        makeParticle(0.0f, 0.0f, 5.0f, 0.25f),
        makeParticle(0.0f, 0.0f, 5.0f, 0.25f),
    };
    full_renderer::ParticleBatchDesc particleBatch;
    particleBatch.particles = particles;
    particleBatch.particleCount = 3;
    particleBatch.sortMode = full_renderer::ParticleSortMode::ParticleDistanceBackToFront;
    submit.batches = &particleBatch;
    submit.batchCount = 1;
    submit.sortMode = full_renderer::ParticleSortMode::SubmissionOrder;

    const full_renderer::scene::ParticleRenderPlan sortedParticles =
        full_renderer::scene::buildParticleRenderPlan(&submit, input);
    expect(sortedParticles.sortedParticleCount == 3, "particle sorting reports sorted particles", failures);
    expect(sortedParticles.particles[0].positionWorld[2] == 5.0f &&
            sortedParticles.particles[1].positionWorld[2] == 5.0f &&
            sortedParticles.particles[2].positionWorld[2] == 1.0f,
        "particle sorting draws far particles before near particles",
        failures);
}

void softParticleSettingsValidateAndClamp(int& failures)
{
    expect(full_renderer::scene::clampSoftParticleFadeDistanceMeters(-1.0f) == 0.0f,
        "negative soft fade clamps to disabled",
        failures);
    expect(full_renderer::scene::clampSoftParticleFadeDistanceMeters(
               std::numeric_limits<float>::infinity()) == 0.0f,
        "non-finite soft fade clamps to disabled",
        failures);
    expect(full_renderer::scene::clampSoftParticleFadeDistanceMeters(1000.0f) == 100.0f,
        "soft fade clamps to implementation maximum",
        failures);

    full_renderer::Particle particle = makeParticle(0.0f, 0.0f, 0.0f, 0.25f);
    full_renderer::ParticleBatchDesc batch;
    batch.particles = &particle;
    batch.particleCount = 1;
    full_renderer::ParticleSubmitDesc submit;
    submit.enabled = true;
    submit.batches = &batch;
    submit.batchCount = 1;
    submit.softParticlesEnabled = true;
    submit.softParticleFadeDistanceMeters = 0.5f;

    const full_renderer::scene::ParticleRenderPlan plan =
        full_renderer::scene::buildParticleRenderPlan(&submit);
    expect(plan.softParticlesEnabled, "soft particle request is reported in plan", failures);
    expect(plan.softParticleFadeDistanceMeters == 0.5f,
        "soft particle fade distance is copied after validation",
        failures);

    submit.softParticleFadeDistanceMeters = std::numeric_limits<float>::quiet_NaN();
    expect(!full_renderer::scene::isValidParticleSubmitDesc(submit),
        "non-finite soft particle fade descriptor is invalid",
        failures);
}

void clampingAndRepeatedPlanningAreDeterministic(int& failures)
{
    full_renderer::Particle particles[full_renderer::kMaxFrameParticles + 2U] = {};
    for (std::uint32_t index = 0; index < full_renderer::kMaxFrameParticles + 2U; ++index)
    {
        particles[index] = makeParticle(static_cast<float>(index), 0.0f, 0.0f, 0.25f);
    }

    full_renderer::ParticleBatchDesc batch;
    batch.particles = particles;
    batch.particleCount = full_renderer::kMaxFrameParticles + 2U;

    full_renderer::ParticleSubmitDesc submit;
    submit.enabled = true;
    submit.batches = &batch;
    submit.batchCount = 1;

    const full_renderer::scene::ParticleRenderPlan first =
        full_renderer::scene::buildParticleRenderPlan(&submit);
    const full_renderer::scene::ParticleRenderPlan second =
        full_renderer::scene::buildParticleRenderPlan(&submit);
    expect(first.acceptedParticleCount == full_renderer::kMaxFrameParticles,
        "particle plan clamps accepted particles to maximum",
        failures);
    expect(first.rejectedParticleCount == 2U,
        "particle plan reports max-count particle rejects",
        failures);
    expect(first.acceptedBatchCount == second.acceptedBatchCount &&
            first.acceptedParticleCount == second.acceptedParticleCount &&
            first.particles[17].positionWorld[0] == second.particles[17].positionWorld[0],
        "repeated particle planning is deterministic",
        failures);

    full_renderer::ParticleBatchDesc batches[full_renderer::kMaxFrameParticleBatches + 1U] = {};
    for (std::uint32_t index = 0; index < full_renderer::kMaxFrameParticleBatches + 1U; ++index)
    {
        batches[index].particles = particles;
        batches[index].particleCount = 1;
        batches[index].texture = full_renderer::TextureHandle{index + 1U};
    }
    submit.batches = batches;
    submit.batchCount = full_renderer::kMaxFrameParticleBatches + 1U;
    const full_renderer::scene::ParticleRenderPlan batchClampPlan =
        full_renderer::scene::buildParticleRenderPlan(&submit);
    expect(batchClampPlan.acceptedBatchCount == full_renderer::kMaxFrameParticleBatches,
        "particle plan clamps accepted batches to maximum",
        failures);
    expect(batchClampPlan.rejectedBatchCount == 1U,
        "particle plan reports max-count batch rejects",
        failures);
    expect(batchClampPlan.fallbackTextureBatchCount == 0,
        "valid nonzero texture handles are not counted as fallback during CPU planning",
        failures);
}
} // namespace

int main()
{
    int failures = 0;
    disabledAndEmptySubmissionsAreNeutral(failures);
    descriptorValidationRejectsInvalidData(failures);
    renderPlanCopiesAndCountsAcceptedParticles(failures);
    batchBoundsIgnoreInvalidParticles(failures);
    cameraFrustumCullsWholeParticleBatches(failures);
    particleSortingIsDeterministicAndStable(failures);
    softParticleSettingsValidateAndClamp(failures);
    clampingAndRepeatedPlanningAreDeterministic(failures);
    return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
