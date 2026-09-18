param([Parameter(Mandatory=$true)][string]$Name,
      [Parameter(Mandatory=$true)][string]$CacheDirectory,
      [switch]$ShaderLifecycle,
      [string]$BuildDirectory='build')
$ErrorActionPreference='Stop'
if(-not [IO.Path]::IsPathFullyQualified($CacheDirectory)){throw 'Absolute isolated cache directory required'}
$source=(Resolve-Path -LiteralPath "$PSScriptRoot/../..").Path
& "$PSScriptRoot/prepare_probe.ps1" -Name $Name -BuildDirectory $BuildDirectory -ShaderLifecycle:$ShaderLifecycle -A1Only:(!$ShaderLifecycle)
if($LASTEXITCODE -ne 0){throw 'Probe preparation failed'}
$target=Join-Path $source "build/loading/$Name"
$previous=$env:M2_SHADER_CACHE_DIR
try {
    $env:M2_SHADER_CACHE_DIR=$CacheDirectory
    @{CacheDirectory=$CacheDirectory;ExistingEntries=@(Get-ChildItem -LiteralPath $CacheDirectory -Filter '*.shadercache' -ErrorAction SilentlyContinue).Count} |
        ConvertTo-Json | Set-Content -LiteralPath "$target/cache-run.json"
    & "$PSScriptRoot/run_probe.ps1" -Name $Name -ShaderCacheDirectory $CacheDirectory
} finally {
    $env:M2_SHADER_CACHE_DIR=$previous
}
