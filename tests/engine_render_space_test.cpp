#include "engine/renderer_integration/RenderSpace.hpp"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>
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

void expectResult(
    const full_engine::RenderSpaceResult actual,
    const full_engine::RenderSpaceResult expected,
    const char* message,
    std::vector<std::string>& failures)
{
    expect(actual == expected, message, failures);
}

bool nearlyEqual(const float lhs, const float rhs)
{
    return std::fabs(lhs - rhs) < 0.0001f;
}

void expectPosition(
    const full_engine::RenderPosition& actual,
    const full_engine::RenderPosition& expected,
    const char* message,
    std::vector<std::string>& failures)
{
    expect(
        nearlyEqual(actual.x, expected.x) && nearlyEqual(actual.y, expected.y) && nearlyEqual(actual.z, expected.z),
        message,
        failures);
}

void testAbsolutePosition(std::vector<std::string>& failures)
{
    const full_engine::WorldOrigin origin = full_engine::makeAbsoluteOrigin();
    full_engine::RenderPosition position;

    expectResult(
        full_engine::toRenderPosition(origin, {10.0, -20.5, 30.25}, position),
        full_engine::RenderSpaceResult::Success,
        "absolute render position conversion succeeds",
        failures);
    expectPosition(position, {10.0f, -20.5f, 30.25f}, "absolute render position matches world position", failures);
}

void testCameraRelativePosition(std::vector<std::string>& failures)
{
    const full_engine::WorldOrigin origin =
        full_engine::makeCameraRelativeOrigin({1000000000.0, -2000000000.0, 3000000000.0});
    full_engine::RenderPosition position;

    expectResult(
        full_engine::toRenderPosition(origin, {1000000128.0, -1999999744.0, 2999999488.0}, position),
        full_engine::RenderSpaceResult::Success,
        "camera-relative render position conversion succeeds",
        failures);
    expectPosition(position, {128.0f, 256.0f, -512.0f}, "camera-relative conversion produces small floats", failures);
}

void testBounds(std::vector<std::string>& failures)
{
    const full_engine::WorldOrigin origin = full_engine::makeCameraRelativeOrigin({100.0, 200.0, -300.0});
    const full_engine::WorldBounds worldBounds{{90.0, 180.0, -320.0}, {150.0, 230.0, -260.0}};

    full_engine::RenderBounds renderBounds;
    expectResult(
        full_engine::toRenderBounds(origin, worldBounds, renderBounds),
        full_engine::RenderSpaceResult::Success,
        "render bounds conversion succeeds",
        failures);

    expectPosition(renderBounds.min, {-10.0f, -20.0f, -20.0f}, "render bounds min is converted", failures);
    expectPosition(renderBounds.max, {50.0f, 30.0f, 40.0f}, "render bounds max is converted", failures);

    const float extentX = renderBounds.max.x - renderBounds.min.x;
    const float extentY = renderBounds.max.y - renderBounds.min.y;
    const float extentZ = renderBounds.max.z - renderBounds.min.z;
    expect(nearlyEqual(extentX, 60.0f), "render bounds preserve x extent", failures);
    expect(nearlyEqual(extentY, 50.0f), "render bounds preserve y extent", failures);
    expect(nearlyEqual(extentZ, 60.0f), "render bounds preserve z extent", failures);
}

void testNegativeCoordinates(std::vector<std::string>& failures)
{
    const full_engine::WorldOrigin origin = full_engine::makeCameraRelativeOrigin({-100.0, -100.0, -100.0});
    full_engine::RenderPosition position;

    expectResult(
        full_engine::toRenderPosition(origin, {-125.0, -75.0, -150.0}, position),
        full_engine::RenderSpaceResult::Success,
        "negative coordinate conversion succeeds",
        failures);
    expectPosition(position, {-25.0f, 25.0f, -50.0f}, "negative coordinates convert correctly", failures);
}

void testInvalidInputs(std::vector<std::string>& failures)
{
    const double infinity = std::numeric_limits<double>::infinity();
    const full_engine::WorldOrigin origin = full_engine::makeAbsoluteOrigin();
    full_engine::RenderPosition position;
    full_engine::RenderBounds bounds;

    expectResult(
        full_engine::toRenderPosition(origin, {infinity, 0.0, 0.0}, position),
        full_engine::RenderSpaceResult::InvalidArgument,
        "non-finite position is invalid",
        failures);

    const full_engine::WorldOrigin invalidOrigin =
        full_engine::makeCameraRelativeOrigin({0.0, std::numeric_limits<double>::quiet_NaN(), 0.0});
    expectResult(
        full_engine::toRenderPosition(invalidOrigin, {1.0, 2.0, 3.0}, position),
        full_engine::RenderSpaceResult::InvalidArgument,
        "non-finite origin is invalid",
        failures);

    expectResult(
        full_engine::toRenderBounds(origin, {{2.0, 0.0, 0.0}, {1.0, 0.0, 0.0}}, bounds),
        full_engine::RenderSpaceResult::InvalidArgument,
        "inverted bounds are invalid",
        failures);
}

void testLimits(std::vector<std::string>& failures)
{
    const full_engine::WorldOrigin origin = full_engine::makeAbsoluteOrigin();
    full_engine::RenderPosition position;
    full_engine::RenderBounds bounds;
    const full_engine::RenderSpaceLimits limits{100.0};

    expectResult(
        full_engine::toRenderPosition(origin, {100.0, -100.0, 0.0}, position, limits),
        full_engine::RenderSpaceResult::Success,
        "values at render-space limit are accepted",
        failures);
    expectPosition(position, {100.0f, -100.0f, 0.0f}, "limit edge values are preserved", failures);

    expectResult(
        full_engine::toRenderPosition(origin, {100.001, 0.0, 0.0}, position, limits),
        full_engine::RenderSpaceResult::OutOfRange,
        "position beyond render-space limit is rejected",
        failures);

    expectResult(
        full_engine::toRenderBounds(origin, {{-100.0, 0.0, 0.0}, {100.001, 1.0, 1.0}}, bounds, limits),
        full_engine::RenderSpaceResult::OutOfRange,
        "bounds beyond render-space limit are rejected",
        failures);

    expectResult(
        full_engine::toRenderPosition(origin, {1.0, 2.0, 3.0}, position, {0.0}),
        full_engine::RenderSpaceResult::InvalidArgument,
        "zero render-space limit is invalid",
        failures);
    expectResult(
        full_engine::toRenderPosition(origin, {1.0, 2.0, 3.0}, position, {std::numeric_limits<double>::infinity()}),
        full_engine::RenderSpaceResult::InvalidArgument,
        "non-finite render-space limit is invalid",
        failures);
}
} // namespace

int main()
{
    std::vector<std::string> failures;

    testAbsolutePosition(failures);
    testCameraRelativePosition(failures);
    testBounds(failures);
    testNegativeCoordinates(failures);
    testInvalidInputs(failures);
    testLimits(failures);

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
