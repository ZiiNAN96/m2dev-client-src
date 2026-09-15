param(
    [Parameter(Mandatory=$true)][ValidatePattern('^[a-zA-Z0-9_-]+$')][string]$Name,
    [Parameter(Mandatory=$true)][string]$BuildDirectory,
    [switch]$Visible,
    [switch]$PrepareOnly,
    [switch]$Modern,
    [switch]$HDRAtmosphere
)
$ErrorActionPreference='Stop'
$source=(Resolve-Path -LiteralPath "$PSScriptRoot/../..").Path
$original=(Resolve-Path -LiteralPath "$source/../m2dev-client").Path
$build=(Resolve-Path -LiteralPath $BuildDirectory).Path
$target=Join-Path $source "build/f5x/runtime/$Name"
if(Test-Path -LiteralPath $target) {throw 'A fresh F5-X evidence directory is required.'}
New-Item -ItemType Directory -Path "$target/test-root","$target/pack","$target/log","$target/mark","$target/upload" | Out-Null
Copy-Item -LiteralPath "$build/bin/Release/Metin2_Release.exe" -Destination "$target/Metin2_Release.exe"
Copy-Item -LiteralPath "$original/config" -Destination "$target/config" -Recurse
if($Modern) { "VERSION 1`nPRESET 4`nSTYLE 1`nSHADOWS 4`nAO 2" | Set-Content -LiteralPath "$target/config/graphics.cfg" }
if($HDRAtmosphere) {
    if(-not $Modern){throw 'HDRAtmosphere requires Modern.'}
    "BLOOM 1`nMODERN_SKY 1`nHIGH_QUALITY_FOG 1" | Add-Content -LiteralPath "$target/config/graphics.cfg"
}
Copy-Item -LiteralPath "$original/assets/root" -Destination "$target/test-root/root" -Recurse
Copy-Item -LiteralPath "$PSScriptRoot/character_runtime_entry.py" -Destination "$target/test-root/root/prototype.py"
Copy-Item -LiteralPath "$PSScriptRoot/fixtures/f5x" -Destination "$target/test-root/root/f5x" -Recurse
Copy-Item -LiteralPath "$source/build/hx/compiled/vegetation" -Destination "$target/vegetation" -Recurse
foreach($package in Get-ChildItem -LiteralPath "$original/pack" -File) {
    if($package.Name -ne 'root.pck') {New-Item -ItemType HardLink -Path "$target/pack/$($package.Name)" -Target $package.FullName | Out-Null}
}
New-Item -ItemType Junction -Path "$target/bgm" -Target "$original/bgm" | Out-Null
& "$build/bin/Release/PackMaker.exe" --input "$target/test-root/root" --output "$target/pack" *> "$target/package.log"
if($LASTEXITCODE -ne 0) {throw 'Private F5-X root packaging failed.'}
$hash=(Get-FileHash -LiteralPath "$target/Metin2_Release.exe" -Algorithm SHA256).Hash
if($hash -ne (Get-FileHash -LiteralPath "$build/bin/Release/Metin2_Release.exe" -Algorithm SHA256).Hash) {throw 'Release binary copy mismatch.'}
"SourceBinary=$build/bin/Release/Metin2_Release.exe`nSHA256=$hash`nArguments=--renderer-diagnostics" | Set-Content -LiteralPath "$target/artifact.txt"
if($PrepareOnly) {Write-Output "Prepared $target";return}
$style=if($Visible){'Normal'}else{'Hidden'}
$process=Start-Process -FilePath "$target/Metin2_Release.exe" -WorkingDirectory $target -WindowStyle $style -ArgumentList '--renderer-diagnostics' -PassThru
$watch=[Diagnostics.Stopwatch]::StartNew()
try {
while(-not $process.WaitForExit(1000)) {
    if($watch.Elapsed.TotalSeconds -gt 90) {Stop-Process -Id $process.Id;throw 'Owned F5-X smoke exceeded 90 seconds; evidence retained.'}
}
$process.WaitForExit()
} finally {
    if(-not $process.HasExited) {Stop-Process -Id $process.Id -ErrorAction Continue; $process.WaitForExit(5000) | Out-Null}
}
"ExitCode=$($process.ExitCode) Seconds=$($watch.Elapsed.TotalSeconds)" | Set-Content -LiteralPath "$target/exit.txt"
if($process.ExitCode -ne 0) {throw 'Native F5-X client returned a nonzero exit code.'}
$log=Get-Content -LiteralPath "$target/f5x-character-smoke.log" -Raw
if($log -notmatch 'completed stages=9 screenshots=9 frames=[1-9][0-9]*') {throw 'Native character sequence or screenshots incomplete.'}
$actors=Get-Content -LiteralPath "$target/actor-renderer.log" -Raw
foreach($vid in 58000..58019) {
    if($actors -notmatch "submitted: GPU-skinned actor part race=65000\b[^\r\n]*\bvid=$vid\b") {throw "Missing native GPU actor $vid"}
}
$audit=Get-Content -LiteralPath "$target/source-resource-audit.log" -Raw
foreach($field in @('AssetDocuments','AnimationInstances','MeshBindings','AllCPUDeformationCalls','AllCPUDeformationVertices','GPUFallbacks','SkinPreparationFailures','RuntimeSkeletons','RuntimeAnimationClips','VegetationAssets','VegetationInstances')) {
    if($audit -notmatch "\b$field=0\b") {throw "Expected $field=0 on shutdown"}
}
if((Get-Content -LiteralPath "$target/log/syserr.txt" -Raw) -match 'Asset Runtime[^\r\n]*(failed|error=)') {throw 'Asset runtime error in native smoke.'}
Write-Output $log
Write-Output $audit
Write-Output "PASS native character client. Evidence: $target"
