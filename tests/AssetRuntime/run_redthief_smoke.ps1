param([Parameter(Mandatory=$true)][string]$BuildDirectory,
    [ValidatePattern('^[a-zA-Z0-9_-]+$')][string]$Name = 'runtime')
$ErrorActionPreference = 'Stop'
$source = (Resolve-Path -LiteralPath "$PSScriptRoot/../..").Path
$client = (Resolve-Path -LiteralPath "$source/../m2dev-client").Path
$build = (Resolve-Path -LiteralPath $BuildDirectory).Path
$target = "$source/build/f3b/$Name"
if (Test-Path -LiteralPath $target) { throw 'A fresh F3-B runtime directory is required.' }
New-Item -ItemType Directory -Path "$target/test-root", "$target/pack", "$target/log", "$target/mark", "$target/upload" | Out-Null
Copy-Item -LiteralPath "$build/bin/Release/Metin2_Release.exe" -Destination "$target/Metin2_Release.exe"
Copy-Item -LiteralPath "$client/config" -Destination "$target/config" -Recurse
Copy-Item -LiteralPath "$client/assets/root" -Destination "$target/test-root/root" -Recurse
Copy-Item -LiteralPath "$PSScriptRoot/redthief_runtime_entry.py" -Destination "$target/test-root/root/prototype.py"
foreach ($pack in Get-ChildItem -LiteralPath "$client/pack" -Filter '*.pck' -File) {
    if ($pack.Name -ne 'root.pck') { New-Item -ItemType HardLink -Path "$target/pack/$($pack.Name)" -Target $pack.FullName | Out-Null }
}
New-Item -ItemType Junction -Path "$target/bgm" -Target "$client/bgm" | Out-Null
& "$build/bin/Release/PackMaker.exe" --input "$target/test-root/root" --output "$target/pack"
if ($LASTEXITCODE -ne 0) { throw 'Fixture packaging failed.' }
$hash = (Get-FileHash -LiteralPath "$target/Metin2_Release.exe").Hash
if ($hash -ne (Get-FileHash -LiteralPath "$build/bin/Release/Metin2_Release.exe").Hash) { throw 'Binary mismatch.' }
"SHA256=$hash`nArguments=--renderer-diagnostics --animation-runtime=ziinan --gr2-reader=ziinan`nFixture; real login is separate" | Set-Content -LiteralPath "$target/artifact.txt"
$process = Start-Process -FilePath "$target/Metin2_Release.exe" -WorkingDirectory $target -WindowStyle Hidden -ArgumentList @('--renderer-diagnostics', '--animation-runtime=ziinan', '--gr2-reader=ziinan') -PassThru
"PID=$($process.Id)" | Set-Content -LiteralPath "$target/process.txt"
$watch = [Diagnostics.Stopwatch]::StartNew()
while (-not $process.WaitForExit(1000)) {
    if ($watch.Elapsed.TotalSeconds -gt 90) { Stop-Process -Id $process.Id; throw 'Owned fixture exceeded its 90-second limit.' }
}
$process.WaitForExit()
"PID=$($process.Id) ExitCode=$($process.ExitCode) Seconds=$([math]::Round($watch.Elapsed.TotalSeconds,2))" | Tee-Object -FilePath "$target/exit.txt"
if ($process.ExitCode -ne 0) { throw 'Client failed.' }
$fixture = Get-Content -LiteralPath "$target/redthief-smoke.log" -Raw
if ($fixture -notmatch 'completed frames=[1-9][0-9]*,[1-9][0-9]*,[1-9][0-9]*,[1-9][0-9]*,[1-9][0-9]*') { throw 'Fixture stages did not all render.' }
$actors = Get-Content -LiteralPath "$target/actor-renderer.log" -Raw
foreach ($race in @(3505, 3555, 3909)) {
    if ($actors -notmatch "submitted: GPU-skinned actor part race=$race\b") { throw "Missing GPU actor $race" }
}
$startup = Get-Content -LiteralPath "$target/renderer-startup.log" -Raw
foreach ($entry in @('GR2Reader=ziinan', 'AnimationRuntime=ziinan', 'Skinning=gpu', 'SkinningSelection=default')) {
    if (-not $startup.Contains($entry)) { throw "Wrong startup route: $entry" }
}
$audit = Get-Content -LiteralPath "$target/source-resource-audit.log" -Raw
foreach ($field in @('GrannyFileReads', 'ReferencePoseSamples', 'ImportPoseSamples', 'AllCPUDeformationCalls', 'AllCPUDeformationVertices', 'GPUFallbacks', 'GR2ReaderResources', 'RuntimeSkeletons', 'RuntimeAnimationClips', 'IndependentAnimationInstances', 'AnimationRuntimeFailures', 'RetainedImportKeyBytes', 'SourceTextures', 'SourceBuffers', 'AssetDocuments', 'AnimationInstances', 'MeshBindings', 'SkinPreparationFailures', 'SkinMeshes', 'BoneRemaps', 'BonePalettes', 'PrototypeGeometry', 'PrototypePalettes', 'StaticSkinMeshes')) {
    if ([regex]::Matches($audit, "\b$field=0\b").Count -ne 1 -or $audit -match "\b$field=[1-9]") { throw "Expected $field=0" }
}
foreach ($field in @('NativeGR2Reads', 'IndependentPoseSamples', 'GPUFrames')) {
    if ($audit -notmatch "\b$field=[1-9][0-9]*\b") { throw "Missing $field activity" }
}
$errors = Get-Content -LiteralPath "$target/log/syserr.txt" -Raw
if ($errors -match 'Asset Runtime[^\r\n]*(failed|error\s*=)|Not found want to using motion|GetMotionKey.*ERROR') { throw 'Asset or motion load failure.' }
Write-Output $audit
Write-Output 'PASS: three production Redthief races, queued damage/back-damage/spawn, pack loading, GPU rendering and clean shutdown. Real login remains separate.'
