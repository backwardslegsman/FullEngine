#include "engine/world/WorldOrigin.hpp"

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
    const full_engine::WorldOriginResult actual,
    const full_engine::WorldOriginResult expected,
    const char* message,
    std::vector<std::string>& failures)
{
    expect(actual == expected, message, failures);
}

bool nearlyEqual(const double lhs, const double rhs)
{
    return std::fabs(lhs - rhs) < 0.000001;
}

void expectVector(
    const full_engine::WorldVector& actual,
    const full_engine::WorldVector& expected,
    const char* message,
    std::vector<std::string>& failures)
{
    expect(
        nearlyEqual(actual.x, expected.x) && nearlyEqual(actual.y, expected.y) && nearlyEqual(actual.z, expected.z),
        message,
        failures);
}

void expectPosition(
    const full_engine::WorldPosition& actual,
    const full_engine::WorldPosition& expected,
    const char* message,
    std::vector<std::string>& failures)
{
    expect(
        nearlyEqual(actual.x, expected.x) && nearlyEqual(actual.y, expected.y) && nearlyEqual(actual.z, expected.z),
        message,
        failures);
}

void testAbsoluteOrigin(std::vector<std::string>& failures)
{
    const full_engine::WorldOrigin origin = full_engine::makeAbsoluteOrigin();
    expect(origin.mode == full_engine::OriginMode::Absolute, "absolute origin uses absolute mode", failures);
    expectPosition(origin.originWorld, {}, "absolute origin is at world zero", failures);

    const full_engine::WorldPosition world{123.0, -45.5, 678.25};
    full_engine::WorldVector relative;
    expectResult(
        full_engine::rebasePosition(origin, world, relative),
        full_engine::WorldOriginResult::Success,
        "absolute rebase succeeds",
        failures);
    expectVector(relative, {123.0, -45.5, 678.25}, "absolute rebase preserves world position", failures);
}

void testCameraRelativeOrigin(std::vector<std::string>& failures)
{
    const full_engine::WorldPosition camera{1000.0, 25.0, -3000.0};
    const full_engine::WorldOrigin origin = full_engine::makeCameraRelativeOrigin(camera);
    expect(origin.mode == full_engine::OriginMode::CameraRelative, "camera origin uses camera-relative mode", failures);
    expectPosition(origin.originWorld, camera, "camera origin stores supplied world position", failures);

    const full_engine::WorldPosition world{1015.5, 20.0, -2990.0};
    full_engine::WorldVector relative;
    expectResult(
        full_engine::rebasePosition(origin, world, relative),
        full_engine::WorldOriginResult::Success,
        "camera-relative rebase succeeds",
        failures);
    expectVector(relative, {15.5, -5.0, 10.0}, "camera-relative rebase subtracts origin", failures);
}

void testNegativeAndLargeCoordinates(std::vector<std::string>& failures)
{
    const full_engine::WorldPosition camera{-1000000000.0, 250000.0, 3000000000.0};
    const full_engine::WorldPosition world{-999999872.0, 249744.0, 3000000512.0};
    const full_engine::WorldOrigin origin = full_engine::makeCameraRelativeOrigin(camera);

    full_engine::WorldVector relative;
    expectResult(
        full_engine::rebasePosition(origin, world, relative),
        full_engine::WorldOriginResult::Success,
        "large coordinate rebase succeeds",
        failures);
    expectVector(relative, {128.0, -256.0, 512.0}, "large coordinate rebase remains deterministic", failures);
}

void testBoundsRebase(std::vector<std::string>& failures)
{
    const full_engine::WorldOrigin origin = full_engine::makeCameraRelativeOrigin({100.0, 200.0, -50.0});
    const full_engine::WorldBounds bounds{{90.0, 195.0, -80.0}, {130.0, 225.0, -20.0}};

    full_engine::WorldBounds relative;
    expectResult(
        full_engine::rebaseBounds(origin, bounds, relative),
        full_engine::WorldOriginResult::Success,
        "bounds rebase succeeds",
        failures);
    expectPosition(relative.min, {-10.0, -5.0, -30.0}, "bounds min is rebased", failures);
    expectPosition(relative.max, {30.0, 25.0, 30.0}, "bounds max is rebased", failures);

    const double originalExtentX = bounds.max.x - bounds.min.x;
    const double relativeExtentX = relative.max.x - relative.min.x;
    const double originalExtentY = bounds.max.y - bounds.min.y;
    const double relativeExtentY = relative.max.y - relative.min.y;
    const double originalExtentZ = bounds.max.z - bounds.min.z;
    const double relativeExtentZ = relative.max.z - relative.min.z;
    expect(nearlyEqual(originalExtentX, relativeExtentX), "bounds rebase preserves x extent", failures);
    expect(nearlyEqual(originalExtentY, relativeExtentY), "bounds rebase preserves y extent", failures);
    expect(nearlyEqual(originalExtentZ, relativeExtentZ), "bounds rebase preserves z extent", failures);
}

void testInvalidInputs(std::vector<std::string>& failures)
{
    const double infinity = std::numeric_limits<double>::infinity();
    const double nan = std::numeric_limits<double>::quiet_NaN();
    const full_engine::WorldOrigin validOrigin = full_engine::makeAbsoluteOrigin();

    expect(
        !full_engine::isFinite(full_engine::WorldPosition{infinity, 0.0, 0.0}),
        "infinite position is not finite",
        failures);
    expect(
        !full_engine::isFinite(full_engine::WorldPosition{0.0, nan, 0.0}),
        "NaN position is not finite",
        failures);
    expect(!full_engine::isFinite({{2.0, 0.0, 0.0}, {1.0, 0.0, 0.0}}), "inverted bounds are invalid", failures);

    full_engine::WorldVector relative;
    expectResult(
        full_engine::rebasePosition(validOrigin, {infinity, 0.0, 0.0}, relative),
        full_engine::WorldOriginResult::InvalidArgument,
        "non-finite position is rejected",
        failures);

    full_engine::WorldOrigin invalidOrigin = full_engine::makeCameraRelativeOrigin({0.0, nan, 0.0});
    expectResult(
        full_engine::rebasePosition(invalidOrigin, {1.0, 2.0, 3.0}, relative),
        full_engine::WorldOriginResult::InvalidArgument,
        "non-finite origin is rejected",
        failures);

    full_engine::WorldBounds relativeBounds;
    expectResult(
        full_engine::rebaseBounds(validOrigin, {{2.0, 0.0, 0.0}, {1.0, 0.0, 0.0}}, relativeBounds),
        full_engine::WorldOriginResult::InvalidArgument,
        "inverted bounds are rejected",
        failures);
    expectResult(
        full_engine::rebaseBounds(validOrigin, {{0.0, 0.0, 0.0}, {1.0, infinity, 0.0}}, relativeBounds),
        full_engine::WorldOriginResult::InvalidArgument,
        "non-finite bounds are rejected",
        failures);
}
} // namespace

int main()
{
    std::vector<std::string> failures;

    testAbsoluteOrigin(failures);
    testCameraRelativeOrigin(failures);
    testNegativeAndLargeCoordinates(failures);
    testBoundsRebase(failures);
    testInvalidInputs(failures);

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
