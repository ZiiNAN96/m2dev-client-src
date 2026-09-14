param(
    [Parameter(Mandatory=$true)][ValidatePattern('^[a-zA-Z0-9_-]+$')][string]$Name,
    [Parameter(Mandatory=$true)][string]$BuildDirectory,
    [switch]$Visible,
    [switch]$AuditOnly
)
$ErrorActionPreference = 'Stop'
$source = (Resolve-Path -LiteralPath "$PSScriptRoot/../..").Path
$original = (Resolve-Path -LiteralPath "$source/../m2dev-client").Path
$build = (Resolve-Path -LiteralPath $BuildDirectory).Path
$binary = (Resolve-Path -LiteralPath "$build/bin/Release/Metin2_Release.exe").Path
$packMaker = (Resolve-Path -LiteralPath "$build/bin/Release/PackMaker.exe").Path
$assetTool = (Resolve-Path -LiteralPath "$build/tools/AssetTool/Release/ziinan-asset-tool.exe").Path
$rootTemplate = (Resolve-Path -LiteralPath "$original/assets/root").Path
$target = Join-Path $source "build-e2x/runtime/$Name"
if ($AuditOnly) {
    if (-not (Test-Path -LiteralPath $target -PathType Container)) { throw 'Named E2-X evidence directory does not exist.' }
} else {
if (Test-Path -LiteralPath $target) { throw 'A fresh E2-X evidence directory is required.' }
New-Item -ItemType Directory -Path "$target/test-root", "$target/pack", "$target/log", "$target/mark", "$target/upload" | Out-Null
Copy-Item -LiteralPath $binary -Destination "$target/Metin2_Release.exe"
Copy-Item -LiteralPath "$original/config" -Destination "$target/config" -Recurse
Copy-Item -LiteralPath $rootTemplate -Destination "$target/test-root/root" -Recurse
Copy-Item -LiteralPath "$PSScriptRoot/runtime_entry.py" -Destination "$target/test-root/root/prototype.py"
New-Item -ItemType Directory -Path "$target/test-root/root/assettool" | Out-Null
$converted = "$target/test-root/root/assettool/market_stall.glb"
& $assetTool convert "$PSScriptRoot/fixtures/market_stall.obj" $converted --json |
    Tee-Object -FilePath "$target/asset-conversion.json"
if ($LASTEXITCODE -ne 0) { throw 'Fresh OBJ-to-GLB conversion failed.' }
& $assetTool validate $converted --json | Tee-Object -FilePath "$target/asset-validation.json"
if ($LASTEXITCODE -ne 0) { throw 'Fresh GLB failed the E1-X provider validation.' }
@'
ScriptType RaceDataScript
BaseModelFileName "assettool/market_stall.glb"
'@ | Set-Content -LiteralPath "$target/test-root/root/assettool/market_stall.msm" -Encoding ascii
$assetHash = (Get-FileHash -LiteralPath $converted -Algorithm SHA256).Hash
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
"SourceBinary=$binary`nSHA256=$hash`nAssetTool=$assetTool`nConvertedGLB=$converted`nGLB_SHA256=$assetHash`nArguments=--renderer-diagnostics`nAutomated fixture; login/window visuals are separate" |
    Set-Content -LiteralPath "$target/artifact.txt"
$style = if ($Visible) { 'Normal' } else { 'Hidden' }
$process = Start-Process -FilePath "$target/Metin2_Release.exe" -WorkingDirectory $target -WindowStyle $style -ArgumentList '--renderer-diagnostics' -PassThru
Write-Output "E2-X fresh Release PID=$($process.Id) Runtime=$target"
$watch = [Diagnostics.Stopwatch]::StartNew()
while (-not $process.WaitForExit(1000)) {
    $process.Refresh()
    if ($watch.Elapsed.TotalSeconds -gt 150) {
        Stop-Process -Id $process.Id
        throw "Owned E2-X fixture exceeded 150 seconds; evidence retained at $target"
    }
    if (-not $process.HasExited) {
        [pscustomobject]@{seconds=[math]::Round($watch.Elapsed.TotalSeconds,1); privateMB=[math]::Round($process.PrivateMemorySize64/1MB,1); handles=$process.HandleCount} |
            Export-Csv -LiteralPath "$target/resources.csv" -NoTypeInformation -Append
    }
}
$process.WaitForExit()
"PID=$($process.Id) ExitCode=$($process.ExitCode) Seconds=$([math]::Round($watch.Elapsed.TotalSeconds,1))" |
    Tee-Object -FilePath "$target/exit.txt"
if ($process.ExitCode -ne 0) { throw 'E2-X fixture exited with a nonzero code.' }
}
$exitEvidence = Get-Content -LiteralPath "$target/exit.txt" -Raw
if ($exitEvidence -notmatch '^PID=[1-9][0-9]* ExitCode=0 Seconds=[0-9.]+\s*$') {
    throw 'Recorded owned runtime process did not have a verified zero exit code.'
}
$artifact = Get-Content -LiteralPath "$target/artifact.txt" -Raw
$recordedBinaryHash = [regex]::Match($artifact, '(?m)^SHA256=([A-Fa-f0-9]{64})\r?$').Groups[1].Value
$recordedAssetHash = [regex]::Match($artifact, '(?m)^GLB_SHA256=([A-Fa-f0-9]{64})\r?$').Groups[1].Value
if (-not $recordedBinaryHash -or $recordedBinaryHash -ne (Get-FileHash -LiteralPath $binary -Algorithm SHA256).Hash -or
    $recordedBinaryHash -ne (Get-FileHash -LiteralPath "$target/Metin2_Release.exe" -Algorithm SHA256).Hash) {
    throw 'Recorded runtime copy does not match its completed Release binary.'
}
if (-not $recordedAssetHash -or $recordedAssetHash -ne (Get-FileHash -LiteralPath "$target/test-root/root/assettool/market_stall.glb" -Algorithm SHA256).Hash) {
    throw 'Recorded converted GLB hash does not match the private packaged source.'
}
$fixture = Get-Content -LiteralPath "$target/asset-runtime-smoke.log" -Raw
if ($fixture -notmatch 'completed phases=3 frames=[1-9][0-9]*') { throw 'Three rendered fixture phases did not complete.' }
foreach ($scene in @('phase=0 map=a1 mount=0 actors=5', 'phase=1 map=b1 mount=20104 actors=5', 'phase=2 map=a1 mount=0 actors=5')) {
    if (-not $fixture.Contains($scene)) { throw "Required original scene missing: $scene" }
}
foreach ($phase in 0..2) {
    if (-not $fixture.Contains("converted phase=$phase race=57999 vid=57999 source=market_stall.obj glb=assettool/market_stall.glb")) {
        throw "Converted GLB instance missing in phase $phase."
    }
    foreach ($step in 0..2) {
        if ($fixture -notmatch "transition phase=$phase step=$step") { throw 'Near/far/near sequence incomplete.' }
    }
    foreach ($distance in @('far', 'near')) {
        $shot = [regex]::Match($fixture, "screenshot phase=$phase camera=$distance success=1 file=(?<file>e2x-world-$phase-$distance-[^\r\n]+)")
        if (-not $shot.Success) { throw "Required phase $phase $distance screenshot did not succeed." }
        $shotPath = Join-Path $target $shot.Groups['file'].Value
        if (-not (Test-Path -LiteralPath $shotPath -PathType Leaf) -or (Get-Item -LiteralPath $shotPath).Length -eq 0) {
            throw "Required phase $phase $distance screenshot artifact is missing or empty."
        }
    }
}
$startup = Get-Content -LiteralPath "$target/renderer-startup.log" -Raw
if (-not $startup.Contains('Skinning=gpu') -or -not $startup.Contains('SkinningSelection=default')) { throw 'Production GPU default was not observed.' }
$audit = Get-Content -LiteralPath "$target/source-resource-audit.log" -Raw
foreach ($field in @('SourceTextures', 'SourceBuffers', 'AssetDocuments', 'AnimationInstances', 'MeshBindings', 'SkinPreparationFailures', 'AllCPUDeformationCalls', 'AllCPUDeformationVertices', 'GPUFallbacks', 'SkinMeshes', 'BoneRemaps', 'BonePalettes', 'PrototypeGeometry', 'PrototypePalettes', 'StaticSkinMeshes')) {
    $observations = [regex]::Matches($audit, "\b$field=(?<value>[0-9]+)\b")
    if ($observations.Count -ne 1 -or $observations[0].Groups['value'].Value -ne '0') { throw "Expected exactly one $field=0 in the fresh shutdown audit." }
}
if ($audit -notmatch '\bGPUFrames=[1-9][0-9]*') { throw 'GPU skinning never rendered.' }
$worldLog = Get-Content -LiteralPath "$target/static-object-adapter.log" -Raw
foreach ($submission in @('static rigid diffuse', 'static camera blocker')) {
    if ($worldLog -notmatch "(?m)^submitted: $submission file=.+") { throw "Required world rendering path did not submit: $submission" }
}
$actorLog = Get-Content -LiteralPath "$target/actor-renderer.log" -Raw
# The existing bridge labels every non-GPU Body as CPU-skinned, including rigid bodies.
# The file identity, zero deform vertices, rigid material submission and global counters prove the actual path.
if ($actorLog -notmatch '(?m)^submitted: CPU-skinned main body race=57999 file=assettool[\\/]market_stall\.glb\b[^\r\n]*\bvid=57999\b[^\r\n]*\bdeform_vertices=0\b[^\r\n]*\brigid_vertices=[1-9][0-9]*\b') {
    throw 'Fresh converted GLB did not submit its rigid main body through the normal actor renderer.'
}
if ($actorLog -notmatch '(?m)^material group=[0-9]+[^\r\n]*\brigid=1\b[^\r\n]*\brace=57999\b') {
    throw 'Converted GLB has no rigid material group draw evidence.'
}
if ($actorLog -match '(?m)^(ERROR:|excluded:)[^\r\n]*\brace=57999\b') {
    throw 'Converted GLB renderer reported an error or excluded draw.'
}
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
    '(?m)^submitted: GPU-skinned actor part race=0\b[^\r\n]*\bvid=57900\b[^\r\n]*\bpart=0\b[^\r\n]*\bcategory=4\b'
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
Write-Output 'PASS: fresh OBJ -> tool -> private GLB pack -> AssetRuntime rigid NPC -> Diligent; parallel original GR2/GPU world/actors; shutdown counters zero. Login, resize and visual comparison remain separate.'
