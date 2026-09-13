# ZiiNAN: Diligent GPU skinning prototype; one fresh private runtime per test.
param([Parameter(Mandatory=$true)][ValidatePattern('^[a-zA-Z0-9_-]+$')][string]$Name,
      [ValidateSet('default','cpu','gpu','gpu-prototype')][string]$Skinning='cpu',
      [ValidateSet('Release','Debug')][string]$Configuration='Release',
      [switch]$OriginalWorld, [switch]$Visible, [switch]$Coverage, [switch]$Normal, [switch]$Stability, [switch]$Benchmark, [switch]$Production)
$ErrorActionPreference='Stop'
$source=(Resolve-Path -LiteralPath "$PSScriptRoot/../..").Path
$original=(Resolve-Path -LiteralPath "$source/../m2dev-client").Path
# ZiiNAN: GPU skinning stability validation
if($Stability -and $OriginalWorld) { throw 'Stability requires its own fixture or a normal login.' }
if($Benchmark -and ($OriginalWorld -or $Coverage -or $Normal -or $Stability)) { throw 'Benchmark requires its own fixed fixture.' }
if($Production -and !$Normal -and !$Stability) { throw 'Production run requires normal login or stability fixture.' }
$phase=if($Benchmark -or $Production) { 'phase-b6x' } elseif($Stability) { 'phase-b5x' } elseif($Coverage) { 'phase-b4x' } else { 'phase-b3' }
$target="$source/build/$phase/$Name"
if(Test-Path -LiteralPath $target) { throw 'Fresh evidence directory required.' }
New-Item -ItemType Directory -Path $target | Out-Null
Copy-Item -LiteralPath "$source/build/bin/$Configuration/Metin2_$Configuration.exe" -Destination "$target/Metin2_Release.exe"
$configurationRoot=if($Normal) { "$original/config" } else { "$source/build/milestone13c/world-release-final/default-runtime/config" }
Copy-Item -LiteralPath $configurationRoot -Destination "$target/config" -Recurse
if($Normal) {
    New-Item -ItemType Junction -Path "$target/pack" -Target "$original/pack" | Out-Null
} elseif($OriginalWorld) {
    New-Item -ItemType Junction -Path "$target/pack" -Target "$source/build/milestone13c/world-release-final/pack" | Out-Null
} else {
    New-Item -ItemType Directory -Path "$target/test-root","$target/pack" | Out-Null
    Copy-Item -LiteralPath "$source/build/milestone13c/world-release-final/test-root/root" -Destination "$target/test-root/root" -Recurse
    $fixture=if($Benchmark) { 'gpu_skinning_benchmark_entry.py' } elseif($Stability) { 'gpu_skinning_stability_entry.py' } elseif($Coverage) { 'gpu_skinning_coverage_entry.py' } else { 'gpu_skinning_entry.py' }
    Copy-Item -LiteralPath "$PSScriptRoot/$fixture" -Destination "$target/test-root/root/prototype.py"
    if($Production -and $Stability) {
        Copy-Item -LiteralPath "$PSScriptRoot/gpu_skinning_stability_entry.py" -Destination "$target/test-root/root/gpu_skinning_stability_entry.py"
        Copy-Item -LiteralPath "$PSScriptRoot/gpu_skinning_production_entry.py" -Destination "$target/test-root/root/prototype.py"
    }
    foreach($package in Get-ChildItem -LiteralPath "$original/pack" -File) {
        if($package.Name -ne 'root.pck') { New-Item -ItemType HardLink -Path "$target/pack/$($package.Name)" -Target $package.FullName | Out-Null }
    }
    & "$source/build/bin/Release/PackMaker.exe" --input "$target/test-root/root" --output "$target/pack"
    if($LASTEXITCODE) { throw 'Private fixture packaging failed.' }
}
New-Item -ItemType Junction -Path "$target/bgm" -Target "$original/bgm" | Out-Null
$arguments=@("--skinning=$Skinning")
if($Skinning -eq 'default') { $arguments=@() }
$style=if($Visible) { 'Normal' } else { 'Hidden' }
$start=@{FilePath="$target/Metin2_Release.exe";WorkingDirectory=$target;WindowStyle=$style;PassThru=$true}
if($arguments.Count) { $start.ArgumentList=$arguments }
$process=Start-Process @start
Write-Output "$phase private $Skinning $Configuration test PID=$($process.Id)"
$watch=[System.Diagnostics.Stopwatch]::StartNew()
while(-not $process.HasExited) {
    $process.Refresh()
    if(-not $process.HasExited) {
        [pscustomobject]@{seconds=[math]::Round($watch.Elapsed.TotalSeconds,1);privateMB=[math]::Round($process.PrivateMemorySize64/1MB,1);workingMB=[math]::Round($process.WorkingSet64/1MB,1);handles=$process.HandleCount} |
            Export-Csv -LiteralPath "$target/resources.csv" -NoTypeInformation -Append
    }
    if($process.WaitForExit(5000)) { break }
}
$process.WaitForExit()
"PID=$($process.Id) ExitCode=$($process.ExitCode) Seconds=$([math]::Round($watch.Elapsed.TotalSeconds,1))" |
    Tee-Object -FilePath "$target/exit.txt"
if($process.ExitCode -ne 0) { throw 'World test process failed.' }
$phaseLog=if($Benchmark) { 'gpu-skinning-benchmark-test.log' } elseif($Stability) { 'gpu-skinning-stability-test.log' } elseif($OriginalWorld) { 'special-world-test.log' } elseif($Coverage) { 'gpu-skinning-coverage-test.log' } else { 'gpu-skinning-test.log' }
$expectedPhases=if($Benchmark) { 5 } elseif($Stability) { 12 } else { 6 }
if(!$Normal -and !(Get-Content -LiteralPath "$target/$phaseLog" -Raw).Contains("completed phases=$expectedPhases")) { throw 'World sequence incomplete.' }
$audit=Get-Content -LiteralPath "$target/source-resource-audit.log" -Raw
if(!$audit.Contains('SkinMeshes=0 BoneRemaps=0 BonePalettes=0') -or !$audit.Contains('SkinPreparationFailures=0') -or
   !$audit.Contains('PrototypeGeometry=0 PrototypePalettes=0')) { throw 'Skinning ownership/preparation check failed.' }
if($Skinning -ne 'cpu' -and !$OriginalWorld -and $audit -notmatch 'GPUFrames=[1-9][0-9]*') { throw 'GPU skinning never rendered; test invalid.' }
if($Skinning -eq 'cpu' -and $audit -notmatch '\bGPUFrames=0\b') { throw 'Explicit CPU unexpectedly used GPU skinning.' }
if($Production) {
    $startup=Get-Content -LiteralPath "$target/renderer-startup.log" -Raw
    if($Skinning -eq 'default' -and (!$startup.Contains('Skinning=gpu') -or !$startup.Contains('SkinningSelection=default'))) { throw 'Production default not observed.' }
    if($audit -notmatch '\bGPUFallbacks=0\b') { throw 'Unexpected fallback.' }
}
Write-Output $audit
if(($Coverage -or $Stability) -and !$audit.Contains('StaticSkinMeshes=0')) { throw 'Shared static skin resources not released.' }
Write-Output "$phase world test PASS"
