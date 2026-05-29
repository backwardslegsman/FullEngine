#include "renderer/scene/Frustum.hpp"
#include "renderer/scene/Math.hpp"

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

full_renderer::Aabb makeBounds(
    const float minX,
    const float minY,
    const float minZ,
    const float maxX,
    const float maxY,
    const float maxZ)
{
    full_renderer::Aabb bounds;
    bounds.min[0] = minX;
    bounds.min[1] = minY;
    bounds.min[2] = minZ;
    bounds.max[0] = maxX;
    bounds.max[1] = maxY;
    bounds.max[2] = maxZ;
    return bounds;
}

void identity(float out[16])
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

void aabbValidation(int& failures)
{
    expect(full_renderer::scene::isValidAabb(makeBounds(-1.0f, -1.0f, -1.0f, 1.0f, 1.0f, 1.0f)),
        "valid AABB is accepted",
        failures);
    expect(!full_renderer::scene::isValidAabb(makeBounds(2.0f, -1.0f, -1.0f, 1.0f, 1.0f, 1.0f)),
        "inverted AABB is rejected",
        failures);

    full_renderer::Aabb bad = makeBounds(-1.0f, -1.0f, -1.0f, 1.0f, 1.0f, 1.0f);
    bad.max[0] = std::numeric_limits<float>::infinity();
    expect(!full_renderer::scene::isValidAabb(bad),
        "non-finite AABB is rejected",
        failures);
}

void frustumIntersection(int& failures)
{
    float viewProjection[16];
    identity(viewProjection);
    const full_renderer::scene::Frustum frustum =
        full_renderer::scene::extractFrustumFromViewProjection(viewProjection);

    expect(full_renderer::scene::intersects(
            frustum,
            makeBounds(-0.5f, -0.5f, -0.5f, 0.5f, 0.5f, 0.5f)),
        "center AABB intersects identity frustum",
        failures);
    expect(full_renderer::scene::intersects(
            frustum,
            makeBounds(0.5f, 0.5f, 0.5f, 1.5f, 1.5f, 1.5f)),
        "partially overlapping AABB intersects identity frustum",
        failures);
    expect(!full_renderer::scene::intersects(
            frustum,
            makeBounds(2.0f, 2.0f, 2.0f, 3.0f, 3.0f, 3.0f)),
        "outside AABB is culled by identity frustum",
        failures);
}
} // namespace

int main()
{
    int failures = 0;
    aabbValidation(failures);
    frustumIntersection(failures);
    return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
