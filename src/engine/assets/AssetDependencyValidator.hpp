#pragma once

#include "engine/assets/AssetCatalog.hpp"

#include <cstdint>

namespace full_engine
{
/** @brief Validation result for resolving one asset record's declared dependencies. */
enum class AssetDependencyValidationResult
{
    Success,
    InvalidAssetRecord,
    MissingDependency,
    WrongDependencyKind,
};

/**
 * @brief Detailed result for generic asset dependency validation.
 *
 * Dependency validation is renderer-free and checks catalog metadata only. It
 * does not load assets, resolve renderer handles, prove resource liveness, or
 * mutate the catalog. `dependencyIndex` is set to `invalidDependencyIndex`
 * unless a specific active dependency caused the failure.
 */
struct AssetDependencyValidation
{
    static constexpr std::uint32_t invalidDependencyIndex = static_cast<std::uint32_t>(-1);

    AssetDependencyValidationResult result = AssetDependencyValidationResult::Success;
    AssetRecordValidationResult recordValidation = AssetRecordValidationResult::Success;
    std::uint32_t dependencyIndex = invalidDependencyIndex;
};

/**
 * @brief Validates that an asset record's active dependencies exist by kind.
 *
 * The source record is structurally validated first with `validateAssetRecord`.
 * Each active dependency must be present in `catalog` and have the declared
 * `AssetKind`. Inactive dependency array entries are ignored.
 */
AssetDependencyValidation validateAssetDependencies(const AssetRecord& record, const AssetCatalog& catalog);
} // namespace full_engine
