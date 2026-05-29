[CmdletBinding()]
param(
    [string] $CMakePath = "",
    [string] $Triplet = "x64-windows",
    [switch] $InstallDependencies,
    [switch] $SkipShaderValidation
)

$ErrorActionPreference = "Stop"

$RepoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path
$VcpkgRoot = Join-Path $RepoRoot "external\vcpkg"
$VcpkgInstalledDir = Join-Path $RepoRoot "build\vcpkg_installed"

function Get-CMakeExecutable {
    if ($CMakePath -ne "") {
        return $CMakePath
    }

    if ($env:CMAKE_EXE -and (Test-Path -LiteralPath $env:CMAKE_EXE)) {
        return $env:CMAKE_EXE
    }

    $visualStudioCMake = "C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
    if (Test-Path -LiteralPath $visualStudioCMake) {
        return $visualStudioCMake
    }

    return "cmake"
}

function Get-CTestExecutable {
    param([string] $ResolvedCMake)

    if ($ResolvedCMake -ne "cmake") {
        $candidate = Join-Path (Split-Path -Parent $ResolvedCMake) "ctest.exe"
        if (Test-Path -LiteralPath $candidate) {
            return $candidate
        }
    }

    return "ctest"
}

function Invoke-ExternalStep {
    param(
        [string] $Name,
        [string] $FilePath,
        [string[]] $Arguments,
        [string] $WorkingDirectory = $RepoRoot
    )

    Write-Host "::group::$Name"
    Push-Location $WorkingDirectory
    try {
        Write-Host "> $FilePath $($Arguments -join ' ')"
        & $FilePath @Arguments
        if ($LASTEXITCODE -ne 0) {
            throw "$Name failed with exit code $LASTEXITCODE."
        }
    }
    finally {
        Pop-Location
        Write-Host "::endgroup::"
    }
}

function Ensure-VcpkgDependencies {
    if (-not $InstallDependencies) {
        return
    }

    $vcpkgExe = Join-Path $VcpkgRoot "vcpkg.exe"
    if (-not (Test-Path -LiteralPath $vcpkgExe)) {
        $bootstrap = Join-Path $VcpkgRoot "bootstrap-vcpkg.bat"
        if (-not (Test-Path -LiteralPath $bootstrap)) {
            if (Test-Path -LiteralPath $VcpkgRoot) {
                $existingEntries = @(Get-ChildItem -Force -LiteralPath $VcpkgRoot -ErrorAction SilentlyContinue)
                if ($existingEntries.Count -gt 0) {
                    throw "vcpkg.exe was not found and $VcpkgRoot is not an empty bootstrap-able vcpkg checkout."
                }
            }

            Invoke-ExternalStep `
                -Name "Clone vcpkg" `
                -FilePath "git" `
                -Arguments @("clone", "https://github.com/microsoft/vcpkg", $VcpkgRoot)
        }

        if (-not (Test-Path -LiteralPath $bootstrap)) {
            throw "vcpkg.exe was not found and bootstrap-vcpkg.bat is missing at $bootstrap."
        }

        Invoke-ExternalStep `
            -Name "Bootstrap vcpkg" `
            -FilePath $bootstrap `
            -Arguments @("-disableMetrics")
    }

    $env:VCPKG_ROOT = $VcpkgRoot
    Invoke-ExternalStep `
        -Name "Install vcpkg dependencies" `
        -FilePath $vcpkgExe `
        -Arguments @("install", "--triplet", $Triplet, "--x-install-root=$VcpkgInstalledDir")
}

$cmake = Get-CMakeExecutable
$ctest = Get-CTestExecutable -ResolvedCMake $cmake
Ensure-VcpkgDependencies

Invoke-ExternalStep `
    -Name "Configure tests-debug" `
    -FilePath $cmake `
    -Arguments @("--fresh", "--preset", "tests-debug")
Invoke-ExternalStep `
    -Name "Build tests-debug" `
    -FilePath $cmake `
    -Arguments @("--build", "--preset", "tests-debug")
Invoke-ExternalStep `
    -Name "Run tests-debug" `
    -FilePath $ctest `
    -Arguments @("--preset", "tests-debug")

Invoke-ExternalStep `
    -Name "Configure library-only-debug" `
    -FilePath $cmake `
    -Arguments @("--fresh", "--preset", "library-only-debug")
Invoke-ExternalStep `
    -Name "Build library-only-debug" `
    -FilePath $cmake `
    -Arguments @("--build", "--preset", "library-only-debug")

Invoke-ExternalStep `
    -Name "Configure no-debug-ui-debug" `
    -FilePath $cmake `
    -Arguments @("--fresh", "--preset", "no-debug-ui-debug")
Invoke-ExternalStep `
    -Name "Build no-debug-ui-debug" `
    -FilePath $cmake `
    -Arguments @("--build", "--preset", "no-debug-ui-debug")

Invoke-ExternalStep `
    -Name "Configure package-debug" `
    -FilePath $cmake `
    -Arguments @("--fresh", "--preset", "package-debug")
Invoke-ExternalStep `
    -Name "Build package-debug" `
    -FilePath $cmake `
    -Arguments @("--build", "--preset", "package-debug")
Invoke-ExternalStep `
    -Name "Install package-debug" `
    -FilePath $cmake `
    -Arguments @("--install", "out/build/package-debug", "--config", "Debug", "--prefix", "out/install/package-debug")
Invoke-ExternalStep `
    -Name "Configure consumer-smoke-debug" `
    -FilePath $cmake `
    -Arguments @("--fresh", "--preset", "consumer-smoke-debug") `
    -WorkingDirectory (Join-Path $RepoRoot "tests\consumer_smoke")
Invoke-ExternalStep `
    -Name "Build consumer-smoke-debug" `
    -FilePath $cmake `
    -Arguments @("--build", "--preset", "consumer-smoke-debug") `
    -WorkingDirectory (Join-Path $RepoRoot "tests\consumer_smoke")
Invoke-ExternalStep `
    -Name "Run consumer-smoke-debug" `
    -FilePath (Join-Path $RepoRoot "out\build\consumer-smoke-debug\Debug\full_renderer_consumer_smoke.exe") `
    -Arguments @()

Invoke-ExternalStep `
    -Name "Configure package-no-debug-ui" `
    -FilePath $cmake `
    -Arguments @("--fresh", "--preset", "package-no-debug-ui")
Invoke-ExternalStep `
    -Name "Build package-no-debug-ui" `
    -FilePath $cmake `
    -Arguments @("--build", "--preset", "package-no-debug-ui")
Invoke-ExternalStep `
    -Name "Install package-no-debug-ui" `
    -FilePath $cmake `
    -Arguments @("--install", "out/build/package-no-debug-ui", "--config", "Debug", "--prefix", "out/install/package-no-debug-ui")
Invoke-ExternalStep `
    -Name "Configure consumer-smoke-no-debug-ui" `
    -FilePath $cmake `
    -Arguments @("--fresh", "--preset", "consumer-smoke-no-debug-ui") `
    -WorkingDirectory (Join-Path $RepoRoot "tests\consumer_smoke")
Invoke-ExternalStep `
    -Name "Build consumer-smoke-no-debug-ui" `
    -FilePath $cmake `
    -Arguments @("--build", "--preset", "consumer-smoke-no-debug-ui") `
    -WorkingDirectory (Join-Path $RepoRoot "tests\consumer_smoke")
Invoke-ExternalStep `
    -Name "Run consumer-smoke-no-debug-ui" `
    -FilePath (Join-Path $RepoRoot "out\build\consumer-smoke-no-debug-ui\Debug\full_renderer_consumer_smoke.exe") `
    -Arguments @()

if (-not $SkipShaderValidation) {
    Invoke-ExternalStep `
        -Name "Configure shader-validation" `
        -FilePath $cmake `
        -Arguments @("--fresh", "--preset", "shader-validation")
    Invoke-ExternalStep `
        -Name "Build shader-validation" `
        -FilePath $cmake `
        -Arguments @("--build", "--preset", "shader-validation")
}
