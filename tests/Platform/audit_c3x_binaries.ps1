[CmdletBinding()]
param(
    [ValidateNotNullOrEmpty()]
    [ValidateSet('Release', 'Debug')]
    [string[]]$Configuration = @('Release', 'Debug'),
    [string]$BuildDirectory,
    [Parameter(Mandatory=$true)][string]$DumpbinPath
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

# ZiiNAN: Cross-platform bootstrap
# This audit reads only artifacts from the clean C3-X build and writes evidence beside that build.
$repositoryRoot = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '../..')).Path
if (-not $BuildDirectory) { $BuildDirectory = Join-Path $repositoryRoot 'build-c3x/windows' }
$buildRoot = (Resolve-Path -LiteralPath $BuildDirectory).Path
$evidenceDirectory = Join-Path $buildRoot 'evidence'
$dumpbin = (Resolve-Path -LiteralPath $DumpbinPath).Path

if (-not (Test-Path -LiteralPath $dumpbin -PathType Leaf)) {
    throw "Required dumpbin was not found at '$dumpbin'."
}

function Get-AuditFiles([string]$BuildConfiguration) {
    $locations = @(
        [pscustomobject]@{ Scope = 'bin'; Directory = Join-Path $buildRoot "bin/$BuildConfiguration"; Filter = '*.exe' },
        [pscustomobject]@{ Scope = 'platform-test'; Directory = Join-Path $buildRoot "tests/Platform/$BuildConfiguration"; Filter = '*.exe' },
        [pscustomobject]@{ Scope = 'renderer-test'; Directory = Join-Path $buildRoot "tests/Renderer/$BuildConfiguration"; Filter = '*.exe' },
        [pscustomobject]@{ Scope = 'diligent-d3d11'; Directory = Join-Path $buildRoot "_deps/diligentcore-build/Graphics/GraphicsEngineD3D11/$BuildConfiguration"; Filter = 'GraphicsEngineD3D11*.dll' }
    )

    $files = [System.Collections.Generic.List[object]]::new()
    foreach ($location in $locations) {
        if (-not (Test-Path -LiteralPath $location.Directory -PathType Container)) {
            throw "Missing clean-build artifact directory '$($location.Directory)'. Build $BuildConfiguration before running this audit."
        }

        $matches = @(Get-ChildItem -LiteralPath $location.Directory -File -Filter $location.Filter | Sort-Object FullName)
        if ($matches.Count -eq 0) {
            throw "No '$($location.Filter)' artifacts found in '$($location.Directory)'."
        }

        foreach ($match in $matches) {
            $resolvedFile = (Resolve-Path -LiteralPath $match.FullName).Path
            $buildPrefix = $buildRoot.TrimEnd('\') + '\'
            if (-not $resolvedFile.StartsWith($buildPrefix, [System.StringComparison]::OrdinalIgnoreCase)) {
                throw "Refusing artifact outside the selected build tree: '$resolvedFile'."
            }
            $files.Add([pscustomobject]@{
                Configuration = $BuildConfiguration
                Scope = $location.Scope
                Path = $resolvedFile
            })
        }
    }
    return $files
}

function Invoke-Dumpbin([string]$Option, [string]$Path) {
    $output = @(& $dumpbin /nologo $Option $Path 2>&1)
    if ($LASTEXITCODE -ne 0) {
        throw "dumpbin $Option failed for '$Path'."
    }
    return $output
}

$artifacts = [System.Collections.Generic.List[object]]::new()
foreach ($buildConfiguration in $Configuration) {
    foreach ($artifact in (Get-AuditFiles $buildConfiguration)) {
        $artifacts.Add($artifact)
    }
}

$manifestRows = [System.Collections.Generic.List[object]]::new()
$importRows = [System.Collections.Generic.List[object]]::new()
$violations = [System.Collections.Generic.List[string]]::new()
$forbiddenImportViolations = [System.Collections.Generic.List[string]]::new()
$forbiddenImportPattern = '^(?:d3d8|d3d9|d3dx[^.]*)\.dll$'

foreach ($artifact in ($artifacts | Sort-Object Configuration, Scope, Path)) {
    $headers = (Invoke-Dumpbin '/headers' $artifact.Path) -join "`n"
    $isAmd64 = $headers -match '(?im)^\s*8664\s+machine\s+\(x64\)\s*$'
    $isPe32Plus = $headers -match '(?im)^\s*20B\s+magic\s+#\s+\(PE32\+\)\s*$'

    $dependencies = [System.Collections.Generic.HashSet[string]]::new([System.StringComparer]::OrdinalIgnoreCase)
    foreach ($line in (Invoke-Dumpbin '/dependents' $artifact.Path)) {
        if ($line -match '^\s+([A-Za-z0-9_.-]+\.dll)\s*$') {
            [void]$dependencies.Add($Matches[1])
        }
    }

    $relativePath = $artifact.Path.Substring($buildRoot.Length).TrimStart('\') -replace '\\', '/'
    foreach ($dependency in ($dependencies | Sort-Object)) {
        $forbidden = $dependency -match $forbiddenImportPattern
        $importRows.Add([pscustomobject]@{
            Configuration = $artifact.Configuration
            Scope = $artifact.Scope
            Binary = $relativePath
            ImportedDll = $dependency
            ForbiddenLegacyGraphicsImport = $forbidden
        })
        if ($forbidden) {
            $violation = "$relativePath imports $dependency"
            $forbiddenImportViolations.Add($violation)
            $violations.Add($violation)
        }
    }

    if (-not $isAmd64) {
        $violations.Add("$relativePath is not AMD64 (dumpbin machine 8664).")
    }
    if (-not $isPe32Plus) {
        $violations.Add("$relativePath is not PE32+ (dumpbin magic 20B).")
    }

    $fileInfo = Get-Item -LiteralPath $artifact.Path
    $manifestRows.Add([pscustomobject]@{
        Configuration = $artifact.Configuration
        Scope = $artifact.Scope
        Binary = $relativePath
        Bytes = $fileInfo.Length
        LastWriteTimeUtc = $fileInfo.LastWriteTimeUtc.ToString('o')
        SHA256 = (Get-FileHash -LiteralPath $artifact.Path -Algorithm SHA256).Hash
        PeFormat = if ($isPe32Plus) { 'PE32+' } else { 'unexpected' }
        Machine = if ($isAmd64) { 'AMD64 (8664)' } else { 'unexpected' }
        DirectImportCount = $dependencies.Count
        ForbiddenLegacyGraphicsImportCount = @($dependencies | Where-Object { $_ -match $forbiddenImportPattern }).Count
    })
}

New-Item -ItemType Directory -Path $evidenceDirectory -Force | Out-Null
$manifestPath = Join-Path $evidenceDirectory 'c3x-binary-audit.csv'
$importsPath = Join-Path $evidenceDirectory 'c3x-binary-imports.csv'
$summaryPath = Join-Path $evidenceDirectory 'c3x-binary-audit-summary.txt'
$manifestRows | Export-Csv -LiteralPath $manifestPath -NoTypeInformation -Encoding UTF8
$importRows | Export-Csv -LiteralPath $importsPath -NoTypeInformation -Encoding UTF8

@(
    "AuditedAtUtc=$([DateTime]::UtcNow.ToString('o'))"
    "BuildRoot=$buildRoot"
    "Configurations=$($Configuration -join ',')"
    "Artifacts=$($manifestRows.Count)"
    "AMD64_PE32Plus=$(@($manifestRows | Where-Object { $_.Machine -eq 'AMD64 (8664)' -and $_.PeFormat -eq 'PE32+' }).Count)"
    "ForbiddenD3D8D3D9D3DXImports=$($forbiddenImportViolations.Count)"
    "Manifest=$manifestPath"
    "Imports=$importsPath"
) | Set-Content -LiteralPath $summaryPath -Encoding UTF8

if ($violations.Count -ne 0) {
    throw "C3-X binary audit failed:`n$($violations -join "`n")"
}

Get-Content -LiteralPath $summaryPath

