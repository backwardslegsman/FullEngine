#pragma once

#include "engine/assets/CookedAssetManifest.hpp"

namespace full_engine
{
/** @brief Result of exporting a cooked asset manifest to JSON Lines. */
enum class CookedAssetManifestExportResult
{
    Success,
    InvalidArgument,
    IoError,
};

/** @brief Result of importing a cooked asset manifest from JSON Lines. */
enum class CookedAssetManifestImportResult
{
    Success,
    InvalidArgument,
    IoError,
    ParseError,
    ValidationError,
};

/**
 * @brief Imported cooked asset manifest plus status and validation detail.
 *
 * On parse or validation failure, `manifest` remains default-empty. When
 * `result` is `ValidationError`, `validation` contains the manifest validation
 * detail from `validateCookedAssetManifest`.
 */
struct CookedAssetManifestImport
{
    CookedAssetManifestImportResult result = CookedAssetManifestImportResult::Success;
    CookedAssetManifest manifest;
    CookedAssetManifestValidation validation;
};

/** @brief Returns a stable diagnostic name for a cooked manifest export result. */
const char* cookedAssetManifestExportResultName(CookedAssetManifestExportResult result) noexcept;

/** @brief Returns a stable diagnostic name for a cooked manifest import result. */
const char* cookedAssetManifestImportResultName(CookedAssetManifestImportResult result) noexcept;

/** @brief Returns a stable serialized name for an engine asset kind. */
const char* assetKindName(AssetKind kind) noexcept;

/**
 * @brief Exports an in-memory cooked asset manifest as deterministic JSON Lines.
 *
 * Generic asset records are written first in manifest order, followed by
 * terrain chunk asset descriptors in manifest order. The export is renderer
 * free and does not validate, load, create, or mutate runtime resources.
 */
CookedAssetManifestExportResult exportCookedAssetManifestJsonLines(
    const CookedAssetManifest& manifest,
    const char* path);

/**
 * @brief Imports a cooked asset manifest from the JSON Lines schema.
 *
 * The importer accepts the current flat object schema, ignores unknown fields,
 * validates the completed manifest, and performs no renderer handle lookup,
 * resource creation, async loading, or caller-owned catalog mutation.
 */
CookedAssetManifestImport importCookedAssetManifestJsonLines(const char* path);
} // namespace full_engine
