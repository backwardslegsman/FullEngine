#include "engine/renderer_integration/LoadedMeshUploadDiagnostics.hpp"

#include <cmath>
#include <limits>

namespace full_engine
{
namespace
{
constexpr float kMinimumNormalLengthSquared = 0.000001f;
constexpr float kDegenerateAreaSquared = 0.000000000001f;

bool hasFiniteValues(const float* const values, const std::size_t count) noexcept
{
    for (std::size_t index = 0; index < count; ++index)
    {
        if (!std::isfinite(values[index]))
        {
            return false;
        }
    }
    return true;
}

bool isUnitRangeColor(const float* const values, const std::size_t count) noexcept
{
    for (std::size_t index = 0; index < count; ++index)
    {
        if (!std::isfinite(values[index]) || values[index] < 0.0f || values[index] > 1.0f)
        {
            return false;
        }
    }
    return true;
}

bool hasUsableNormal(const float normal[3]) noexcept
{
    if (!hasFiniteValues(normal, 3))
    {
        return false;
    }

    const float lengthSquared =
        normal[0] * normal[0] +
        normal[1] * normal[1] +
        normal[2] * normal[2];
    return std::isfinite(lengthSquared) && lengthSquared >= kMinimumNormalLengthSquared;
}

bool hasUsableTangent(const float tangent[4]) noexcept
{
    constexpr float kHandednessTolerance = 0.001f;
    if (!hasFiniteValues(tangent, 4))
    {
        return false;
    }

    const float lengthSquared =
        tangent[0] * tangent[0] +
        tangent[1] * tangent[1] +
        tangent[2] * tangent[2];
    if (!std::isfinite(lengthSquared) || lengthSquared < kMinimumNormalLengthSquared)
    {
        return false;
    }

    return std::fabs(std::fabs(tangent[3]) - 1.0f) <= kHandednessTolerance;
}

bool hasValidVertexData(const LoadedMeshVertex& vertex) noexcept
{
    return hasFiniteValues(vertex.position, 3) &&
        hasUsableNormal(vertex.normal) &&
        hasUsableTangent(vertex.tangent) &&
        hasFiniteValues(vertex.uv0, 2) &&
        isUnitRangeColor(vertex.colorLinear, 4);
}

bool triangleIsDegenerate(
    const LoadedMeshVertex& a,
    const LoadedMeshVertex& b,
    const LoadedMeshVertex& c) noexcept
{
    const float ab[3] = {
        b.position[0] - a.position[0],
        b.position[1] - a.position[1],
        b.position[2] - a.position[2]};
    const float ac[3] = {
        c.position[0] - a.position[0],
        c.position[1] - a.position[1],
        c.position[2] - a.position[2]};
    const float cross[3] = {
        ab[1] * ac[2] - ab[2] * ac[1],
        ab[2] * ac[0] - ab[0] * ac[2],
        ab[0] * ac[1] - ab[1] * ac[0]};
    const float areaSquared =
        cross[0] * cross[0] +
        cross[1] * cross[1] +
        cross[2] * cross[2];
    return !std::isfinite(areaSquared) || areaSquared <= kDegenerateAreaSquared;
}

bool countFitsUint32(const std::size_t count) noexcept
{
    return count <= std::numeric_limits<std::uint32_t>::max();
}

void rememberFirstVertexFailure(
    LoadedMeshUploadDiagnostics& diagnostics,
    const std::size_t vertexIndex) noexcept
{
    if (diagnostics.firstFailure.vertexIndex == LoadedMeshUploadDiagnosticFailure::invalidIndex &&
        vertexIndex <= std::numeric_limits<std::uint32_t>::max())
    {
        diagnostics.firstFailure.vertexIndex = static_cast<std::uint32_t>(vertexIndex);
    }
}

void rememberFirstIndexFailure(
    LoadedMeshUploadDiagnostics& diagnostics,
    const std::size_t indexOffset) noexcept
{
    if (diagnostics.firstFailure.indexOffset == LoadedMeshUploadDiagnosticFailure::invalidIndex &&
        indexOffset <= std::numeric_limits<std::uint32_t>::max())
    {
        diagnostics.firstFailure.indexOffset = static_cast<std::uint32_t>(indexOffset);
    }
}

void rememberFirstTriangleFailure(
    LoadedMeshUploadDiagnostics& diagnostics,
    const std::size_t triangleIndex) noexcept
{
    if (diagnostics.firstFailure.triangleIndex == LoadedMeshUploadDiagnosticFailure::invalidIndex &&
        triangleIndex <= std::numeric_limits<std::uint32_t>::max())
    {
        diagnostics.firstFailure.triangleIndex = static_cast<std::uint32_t>(triangleIndex);
    }
}
} // namespace

const char* loadedMeshUploadDiagnosticStatusName(
    const LoadedMeshUploadDiagnosticStatus status) noexcept
{
    switch (status)
    {
    case LoadedMeshUploadDiagnosticStatus::Valid:
        return "Valid";
    case LoadedMeshUploadDiagnosticStatus::InvalidLoadedPayload:
        return "InvalidLoadedPayload";
    case LoadedMeshUploadDiagnosticStatus::CountOverflow:
        return "CountOverflow";
    case LoadedMeshUploadDiagnosticStatus::InvalidRendererStructure:
        return "InvalidRendererStructure";
    case LoadedMeshUploadDiagnosticStatus::InvalidRendererVertexData:
        return "InvalidRendererVertexData";
    case LoadedMeshUploadDiagnosticStatus::InvalidRendererIndices:
        return "InvalidRendererIndices";
    case LoadedMeshUploadDiagnosticStatus::DegenerateTriangles:
        return "DegenerateTriangles";
    }
    return "Unknown";
}

LoadedMeshUploadDiagnostics diagnoseLoadedMeshUploadContract(
    const LoadedMeshAsset& mesh) noexcept
{
    LoadedMeshUploadDiagnostics diagnostics;
    diagnostics.vertexCount = mesh.vertices.size();
    diagnostics.indexCount = mesh.indices.size();
    diagnostics.payloadValidation = validateLoadedMeshAsset(mesh);

    for (std::size_t vertexIndex = 0; vertexIndex < mesh.vertices.size(); ++vertexIndex)
    {
        if (!hasValidVertexData(mesh.vertices[vertexIndex]))
        {
            ++diagnostics.invalidVertexDataCount;
            rememberFirstVertexFailure(diagnostics, vertexIndex);
        }
    }

    for (std::size_t indexOffset = 0; indexOffset < mesh.indices.size(); ++indexOffset)
    {
        if (static_cast<std::size_t>(mesh.indices[indexOffset]) >= mesh.vertices.size())
        {
            ++diagnostics.invalidIndexCount;
            rememberFirstIndexFailure(diagnostics, indexOffset);
        }
    }

    if (!mesh.vertices.empty() && (mesh.indices.size() % 3U) == 0U)
    {
        for (std::size_t indexOffset = 0; indexOffset < mesh.indices.size(); indexOffset += 3U)
        {
            const std::uint16_t a = mesh.indices[indexOffset + 0U];
            const std::uint16_t b = mesh.indices[indexOffset + 1U];
            const std::uint16_t c = mesh.indices[indexOffset + 2U];
            if (static_cast<std::size_t>(a) >= mesh.vertices.size() ||
                static_cast<std::size_t>(b) >= mesh.vertices.size() ||
                static_cast<std::size_t>(c) >= mesh.vertices.size())
            {
                continue;
            }

            if (triangleIsDegenerate(mesh.vertices[a], mesh.vertices[b], mesh.vertices[c]))
            {
                ++diagnostics.degenerateTriangleCount;
                rememberFirstTriangleFailure(diagnostics, indexOffset / 3U);
            }
        }
    }

    if (diagnostics.payloadValidation != LoadedAssetPayloadValidationResult::Success)
    {
        diagnostics.status = LoadedMeshUploadDiagnosticStatus::InvalidLoadedPayload;
        return diagnostics;
    }

    if (!countFitsUint32(mesh.vertices.size()) || !countFitsUint32(mesh.indices.size()))
    {
        diagnostics.status = LoadedMeshUploadDiagnosticStatus::CountOverflow;
        return diagnostics;
    }

    if (mesh.vertices.empty() || mesh.indices.empty() || (mesh.indices.size() % 3U) != 0U)
    {
        diagnostics.status = LoadedMeshUploadDiagnosticStatus::InvalidRendererStructure;
        return diagnostics;
    }

    if (diagnostics.invalidVertexDataCount > 0U)
    {
        diagnostics.status = LoadedMeshUploadDiagnosticStatus::InvalidRendererVertexData;
        return diagnostics;
    }

    if (diagnostics.invalidIndexCount > 0U)
    {
        diagnostics.status = LoadedMeshUploadDiagnosticStatus::InvalidRendererIndices;
        return diagnostics;
    }

    if (diagnostics.degenerateTriangleCount > 0U)
    {
        diagnostics.status = LoadedMeshUploadDiagnosticStatus::DegenerateTriangles;
        return diagnostics;
    }

    diagnostics.status = LoadedMeshUploadDiagnosticStatus::Valid;
    return diagnostics;
}
} // namespace full_engine
