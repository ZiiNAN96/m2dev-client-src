param(
    [Parameter(Mandatory=$true)][ValidatePattern('^[a-zA-Z0-9_-]+$')][string]$Name,
    [Parameter(Mandatory=$true)][string]$BuildDirectory,
    [ValidateSet('ziinan')][string]$AnimationRuntime = 'ziinan', [switch]$Visible
)
$ErrorActionPreference = 'Stop'
$source = (Resolve-Path -LiteralPath "$PSScriptRoot/../..").Path
$original = (Resolve-Path -LiteralPath "$source/../m2dev-client").Path
$build = (Resolve-Path -LiteralPath $BuildDirectory).Path
$binary = (Resolve-Path -LiteralPath "$build/bin/Release/Metin2_Release.exe").Path
$packMaker = (Resolve-Path -LiteralPath "$build/bin/Release/PackMaker.exe").Path
$rootTemplate = (Resolve-Path -LiteralPath "$original/assets/root").Path
$target = Join-Path $source "build/f1x/runtime/$Name"
if (Test-Path -LiteralPath $target) { throw 'A fresh F1-X evidence directory is required.' }
New-Item -ItemType Directory -Path "$target/test-root", "$target/pack", "$target/log", "$target/mark", "$target/upload" | Out-Null
Copy-Item -LiteralPath $binary -Destination "$target/Metin2_Release.exe"
Copy-Item -LiteralPath "$original/config" -Destination "$target/config" -Recurse
Copy-Item -LiteralPath $rootTemplate -Destination "$target/test-root/root" -Recurse
Copy-Item -LiteralPath "$PSScriptRoot/runtime_entry.py" -Destination "$target/test-root/root/prototype.py"
foreach ($package in Get-ChildItem -LiteralPath "$original/pack" -File) {
    if ($package.Name -ne 'root.pck') {
        New-Item -ItemType HardLink -Path "$target/pack/$($package.Name)" -Target $package.FullName | Out-Null
    }
}
New-Item -ItemType Junction -Path "$target/bgm" -Target "$original/bgm" | Out-Null
& $packMaker --input "$target/test-root/root" --output "$target/pack"
if ($LASTEXITCODE -ne 0) { throw 'Private fixture packaging failed.' }
$hash = (Get-FileHash -LiteralPath $binary -Algorithm SHA256).Hash
if ((Get-FileHash -LiteralPath "$target/Metin2_Release.exe" -Algorithm SHA256).Hash -ne $hash) {
    throw 'Runtime copy differs from the completed Release build.'
}
"SourceBinary=$binary`nSHA256=$hash`nArguments=--renderer-diagnostics --animation-runtime=$AnimationRuntime`nAutomated fixture; login/window visuals are separate" |
    Set-Content -LiteralPath "$target/artifact.txt"
$style = if ($Visible) { 'Normal' } else { 'Hidden' }
$process = Start-Process -FilePath "$target/Metin2_Release.exe" -WorkingDirectory $target -WindowStyle $style -ArgumentList @('--renderer-diagnostics', "--animation-runtime=$AnimationRuntime") -PassThru
Write-Output "F1-X fresh Release PID=$($process.Id) Runtime=$target"
$watch = [Diagnostics.Stopwatch]::StartNew()
while (-not $process.WaitForExit(1000)) {
    $process.Refresh()
    if ($watch.Elapsed.TotalSeconds -gt 100) {
        Stop-Process -Id $process.Id
        throw "Owned F1-X fixture exceeded 100 seconds; evidence retained at $target"
    }
    if (-not $process.HasExited) {
        [pscustomobject]@{seconds=[math]::Round($watch.Elapsed.TotalSeconds,1); privateMB=[math]::Round($process.PrivateMemorySize64/1MB,1); handles=$process.HandleCount} |
            Export-Csv -LiteralPath "$target/resources.csv" -NoTypeInformation -Append
    }
}
$process.WaitForExit()
"PID=$($process.Id) ExitCode=$($process.ExitCode) Seconds=$([math]::Round($watch.Elapsed.TotalSeconds,1))" |
    Tee-Object -FilePath "$target/exit.txt"
if ($process.ExitCode -ne 0) { throw 'F1-X fixture exited with a nonzero code.' }
$fixture = Get-Content -LiteralPath "$target/asset-runtime-smoke.log" -Raw
if ($fixture -notmatch 'completed phases=1 frames=[1-9][0-9]*') { throw 'Rendered fixture did not complete.' }
foreach ($animation in 0..3) {
    if ($fixture -notmatch "rendered animation=$animation frames=[1-9][0-9]*") { throw "Animation stage $animation never rendered." }
}
foreach ($scene in @('phase=0 map=a1 mount=20104 actors=6')) {
    if (-not $fixture.Contains($scene)) { throw "Required original scene missing: $scene" }
}
foreach ($phase in 0..0) {
    foreach ($step in 0..2) {
        if ($fixture -notmatch "transition phase=$phase step=$step") { throw 'Near/far/near sequence incomplete.' }
    }
    foreach ($distance in @('far', 'near')) {
        $shot = [regex]::Match($fixture, "screenshot phase=$phase camera=$distance success=1 file=(?<file>f1x-world-$phase-$distance-[^\r\n]+)")
        if (-not $shot.Success) { throw "Required phase $phase $distance screenshot did not succeed." }
        $shotPath = Join-Path $target $shot.Groups['file'].Value
        if (-not (Test-Path -LiteralPath $shotPath -PathType Leaf) -or (Get-Item -LiteralPath $shotPath).Length -eq 0) {
            throw "Required phase $phase $distance screenshot artifact is missing or empty."
        }
    }
}
$startup = Get-Content -LiteralPath "$target/renderer-startup.log" -Raw
if (-not $startup.Contains('Skinning=gpu') -or -not $startup.Contains('SkinningSelection=default')) { throw 'Production GPU default was not observed.' }
if (-not $startup.Contains("AnimationRuntime=$AnimationRuntime")) { throw 'Requested animation runtime was not used.' }
$audit = Get-Content -LiteralPath "$target/source-resource-audit.log" -Raw
foreach ($field in @('RuntimeSkeletons', 'RuntimeAnimationClips', 'IndependentAnimationInstances', 'AnimationRuntimeFailures', 'RetainedImportKeyBytes', 'SourceTextures', 'SourceBuffers', 'AssetDocuments', 'AnimationInstances', 'MeshBindings', 'SkinPreparationFailures', 'AllCPUDeformationCalls', 'AllCPUDeformationVertices', 'GPUFallbacks', 'SkinMeshes', 'BoneRemaps', 'BonePalettes', 'PrototypeGeometry', 'PrototypePalettes', 'StaticSkinMeshes')) {
    $observations = [regex]::Matches($audit, "\b$field=(?<value>[0-9]+)\b")
    if ($observations.Count -ne 1 -or $observations[0].Groups['value'].Value -ne '0') { throw "Expected exactly one $field=0 in the fresh shutdown audit." }
}
if ($AnimationRuntime -eq 'ziinan' -and ($audit -notmatch '\bIndependentPoseSamples=[1-9][0-9]*' -or $audit -notmatch '\bReferencePoseSamples=0\b')) { throw 'Independent pose sampling / zero SDK pose sampling not proved.' }
if ($audit -notmatch '\bGPUFrames=[1-9][0-9]*') { throw 'GPU skinning never rendered.' }
$worldLog = Get-Content -LiteralPath "$target/static-object-adapter.log" -Raw
foreach ($submission in @('static rigid diffuse', 'static camera blocker')) {
    if ($worldLog -notmatch "(?m)^submitted: $submission file=.+") { throw "Required world rendering path did not submit: $submission" }
}
$actorLog = Get-Content -LiteralPath "$target/actor-renderer.log" -Raw
$races = @(0, 9003, 101, 691, 20101)
for ($index = 0; $index -lt $races.Count; ++$index) {
    $race = $races[$index]
    $vid = 57900 + $index
    if ($actorLog -notmatch "(?m)^submitted: GPU-skinned actor part race=$race\b[^\r\n]*\bvid=$vid\b[^\r\n]*\bpart=0\b") {
        throw "Required original actor did not submit through GPU skinning: race=$race vid=$vid"
    }
}
foreach ($submission in @(
    '(?m)^submitted: rigid attachment race=0\b[^\r\n]*\bvid=57900\b[^\r\n]*\bpart=1\b',
    '(?m)^submitted: GPU-skinned actor part race=0\b[^\r\n]*\bvid=57900\b[^\r\n]*\bpart=4\b',
    '(?m)^submitted: GPU-skinned actor part race=20104\b[^\r\n]*\bpart=0\b[^\r\n]*\bcategory=3\b',
    '(?m)^submitted: GPU-skinned actor part race=0\b[^\r\n]*\bvid=57905\b[^\r\n]*\bpart=0\b[^\r\n]*\bcategory=4\b'
)) {
    if ($actorLog -notmatch $submission) { throw "Required weapon/hair/mount/rider rendering path did not submit: $submission" }
}
$errorLog = Join-Path $target 'log/syserr.txt'
if (Test-Path -LiteralPath $errorLog) {
    $runtimeErrors = @(Select-String -LiteralPath $errorLog -Pattern 'Asset Runtime[^\r\n]*(\bfailed\b|\berror\s*=)' -CaseSensitive:$false)
    if ($runtimeErrors.Count -gt 0) {
        $firstError = $runtimeErrors[0].Line
        throw "Asset Runtime reported $($runtimeErrors.Count) failure(s) in the fresh smoke log. First: $firstError"
    }
}
Write-Output $audit
Write-Output 'PASS: isolated original-asset render/map/lifetime smoke; login, resize and visual comparison remain separate.'


