#include "engine/renderer_integration/LoadedMeshUploadDiagnostics.hpp"

#include <cmath>
#include <iostream>
#include <string>
#include <vector>

namespace
{
void expect(const bool condition, const char* const message, std::vector<std::string>& failures)
{
    if (!condition)
    {
        failures.emplace_back(message);
    }
}

full_engine::AssetId asset(const std::uint64_t value) noexcept
{
    return {value};
}

full_engine::LoadedMeshVertex vertex(const float x, const float y, const float z) noexcept
{
    full_engine::LoadedMeshVertex result;
    result.position[0] = x;
    result.position[1] = y;
    result.position[2] = z;
    result.normal[0] = 0.0f;
    result.normal[1] = 1.0f;
    result.normal[2] = 0.0f;
    result.tangent[0] = 1.0f;
    result.tangent[1] = 0.0f;
    result.tangent[2] = 0.0f;
    result.tangent[3] = 1.0f;
    result.uv0[0] = x;
    result.uv0[1] = y;
    result.colorLinear[0] = 1.0f;
    result.colorLinear[1] = 0.75f;
    result.colorLinear[2] = 0.5f;
    result.colorLinear[3] = 1.0f;
    return result;
}

full_engine::LoadedMeshAsset meshAsset()
{
    full_engine::LoadedMeshAsset mesh;
    mesh.id = asset(10);
    mesh.vertices = {
        vertex(0.0f, 0.0f, 0.0f),
        vertex(1.0f, 0.0f, 0.0f),
        vertex(0.0f, 1.0f, 0.0f)};
    mesh.indices = {0, 1, 2};
    mesh.localBounds.min[0] = 0.0f;
    mesh.localBounds.min[1] = 0.0f;
    mesh.localBounds.min[2] = 0.0f;
    mesh.localBounds.max[0] = 1.0f;
    mesh.localBounds.max[1] = 1.0f;
    mesh.localBounds.max[2] = 0.0f;
    return mesh;
}

void testValidMesh(std::vector<std::string>& failures)
{
    const full_engine::LoadedMeshUploadDiagnostics diagnostics =
        full_engine::diagnoseLoadedMeshUploadContract(meshAsset());
    expect(diagnostics.status == full_engine::LoadedMeshUploadDiagnosticStatus::Valid, "valid mesh diagnoses valid", failures);
    expect(diagnostics.invalidVertexDataCount == 0, "valid mesh has no invalid vertices", failures);
    expect(diagnostics.invalidIndexCount == 0, "valid mesh has no invalid indices", failures);
    expect(diagnostics.degenerateTriangleCount == 0, "valid mesh has no degenerate triangles", failures);
}

void testInvalidVertexData(std::vector<std::string>& failures)
{
    full_engine::LoadedMeshAsset mesh = meshAsset();
    mesh.vertices[1].colorLinear[0] = std::nanf("");
    const full_engine::LoadedMeshUploadDiagnostics diagnostics =
        full_engine::diagnoseLoadedMeshUploadContract(mesh);
    expect(diagnostics.status == full_engine::LoadedMeshUploadDiagnosticStatus::InvalidLoadedPayload, "invalid loaded vertex reports payload failure", failures);
    expect(diagnostics.payloadValidation == full_engine::LoadedAssetPayloadValidationResult::InvalidMeshVertexData, "invalid vertex payload detail is preserved", failures);
    expect(diagnostics.invalidVertexDataCount == 1, "invalid vertex count increments", failures);
    expect(diagnostics.firstFailure.vertexIndex == 1, "first invalid vertex is recorded", failures);
}

void testInvalidIndices(std::vector<std::string>& failures)
{
    full_engine::LoadedMeshAsset mesh = meshAsset();
    mesh.indices[2] = 99;
    const full_engine::LoadedMeshUploadDiagnostics diagnostics =
        full_engine::diagnoseLoadedMeshUploadContract(mesh);
    expect(diagnostics.status == full_engine::LoadedMeshUploadDiagnosticStatus::InvalidLoadedPayload, "invalid index reports payload failure", failures);
    expect(diagnostics.payloadValidation == full_engine::LoadedAssetPayloadValidationResult::InvalidMeshIndices, "invalid index payload detail is preserved", failures);
    expect(diagnostics.invalidIndexCount == 1, "invalid index count increments", failures);
    expect(diagnostics.firstFailure.indexOffset == 2, "first invalid index offset is recorded", failures);
}

void testDegenerateTriangle(std::vector<std::string>& failures)
{
    full_engine::LoadedMeshAsset mesh = meshAsset();
    mesh.vertices[2].position[0] = 2.0f;
    mesh.vertices[2].position[1] = 0.0f;
    mesh.localBounds.max[0] = 2.0f;
    const full_engine::LoadedMeshUploadDiagnostics diagnostics =
        full_engine::diagnoseLoadedMeshUploadContract(mesh);
    expect(diagnostics.status == full_engine::LoadedMeshUploadDiagnosticStatus::DegenerateTriangles, "degenerate triangle reports renderer contract failure", failures);
    expect(diagnostics.payloadValidation == full_engine::LoadedAssetPayloadValidationResult::Success, "degenerate triangle can still be valid loaded payload", failures);
    expect(diagnostics.degenerateTriangleCount == 1, "degenerate triangle count increments", failures);
    expect(diagnostics.firstFailure.triangleIndex == 0, "first degenerate triangle is recorded", failures);
}

void testStatusNames(std::vector<std::string>& failures)
{
    const full_engine::LoadedMeshUploadDiagnosticStatus statuses[] = {
        full_engine::LoadedMeshUploadDiagnosticStatus::Valid,
        full_engine::LoadedMeshUploadDiagnosticStatus::InvalidLoadedPayload,
        full_engine::LoadedMeshUploadDiagnosticStatus::CountOverflow,
        full_engine::LoadedMeshUploadDiagnosticStatus::InvalidRendererStructure,
        full_engine::LoadedMeshUploadDiagnosticStatus::InvalidRendererVertexData,
        full_engine::LoadedMeshUploadDiagnosticStatus::InvalidRendererIndices,
        full_engine::LoadedMeshUploadDiagnosticStatus::DegenerateTriangles};
    for (const full_engine::LoadedMeshUploadDiagnosticStatus status : statuses)
    {
        expect(std::string(full_engine::loadedMeshUploadDiagnosticStatusName(status)) != "Unknown", "mesh diagnostic status name is covered", failures);
    }
}
} // namespace

int main()
{
    std::vector<std::string> failures;
    testValidMesh(failures);
    testInvalidVertexData(failures);
    testInvalidIndices(failures);
    testDegenerateTriangle(failures);
    testStatusNames(failures);

    if (!failures.empty())
    {
        for (const std::string& failure : failures)
        {
            std::cerr << failure << "\n";
        }
        return 1;
    }

    return 0;
}
