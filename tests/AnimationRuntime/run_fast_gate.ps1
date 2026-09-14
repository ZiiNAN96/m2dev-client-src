param(
    [Parameter(Mandatory=$true)][string]$BuildDirectory,
    [ValidateSet('Release','Debug')][string]$Configuration = 'Release'
)
$ErrorActionPreference = 'Stop'
$root = (Resolve-Path -LiteralPath "$PSScriptRoot/../..").Path
$build = (Resolve-Path -LiteralPath $BuildDirectory).Path
$evidence = Join-Path $root 'build/f1x'
New-Item -ItemType Directory -Force -Path $evidence | Out-Null
$testLog = Join-Path $evidence ($Configuration.ToLowerInvariant() + '-fast-tests.log')
$gate = '^(AnimationRuntime|AssetRuntime|AssetTool|Platform)\.|^Renderer\.(StartupOptions|ResourceSource|DiligentD3D11|ProductionGpu|SkinningInstance|NoLegacyArchitecture|HairLodQuick|SkinningBenchmark)$'
$watch = [Diagnostics.Stopwatch]::StartNew()
& ctest --test-dir $build -C $Configuration -R $gate --output-on-failure *> $testLog
$result = $LASTEXITCODE
Get-Content -LiteralPath $testLog -Tail 45
"Configuration=$Configuration ExitCode=$result Seconds=$($watch.Elapsed.TotalSeconds)" |
    Set-Content -LiteralPath (Join-Path $evidence ($Configuration.ToLowerInvariant() + '-fast-result.txt'))
if ($result -ne 0) { throw "F1-X $Configuration gate failed; see $testLog" }
