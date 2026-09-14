param(
    [Parameter(Mandatory=$true)][ValidatePattern('^[a-zA-Z0-9_-]+$')][string]$Name,
    [string]$BuildDirectory = 'build-c3x/windows',
    [ValidateSet('granny','ziinan')][string]$Reader = 'ziinan',
    [ValidateSet('on','off')][string]$Prewarm = 'off',
    [switch]$Manual
)
$ErrorActionPreference = 'Stop'
$source = (Resolve-Path -LiteralPath "$PSScriptRoot/../..").Path
$original = (Resolve-Path -LiteralPath "$source/../m2dev-client").Path
$build = (Resolve-Path -LiteralPath $BuildDirectory).Path
$binary = (Resolve-Path -LiteralPath "$build/bin/Release/Metin2_Release.exe").Path
$target = Join-Path $source "build/f2p/runtime/$Name"
if (Test-Path -LiteralPath $target) { throw 'A fresh performance evidence directory is required.' }
New-Item -ItemType Directory -Path "$target/pack", "$target/log", "$target/mark", "$target/upload" | Out-Null
Copy-Item -LiteralPath $binary -Destination "$target/Metin2_Release.exe"
Copy-Item -LiteralPath "$original/config" -Destination "$target/config" -Recurse
foreach ($package in Get-ChildItem -LiteralPath "$original/pack" -File) {
    if ($Manual -or $package.Name -ne 'root.pck') {
        New-Item -ItemType HardLink -Path "$target/pack/$($package.Name)" -Target $package.FullName | Out-Null
    }
}
New-Item -ItemType Junction -Path "$target/bgm" -Target "$original/bgm" | Out-Null
if (-not $Manual) {
    New-Item -ItemType Directory -Path "$target/test-root" | Out-Null
    Copy-Item -LiteralPath "$original/assets/root" -Destination "$target/test-root/root" -Recurse
    Copy-Item -LiteralPath "$PSScriptRoot/load_warmup_entry.py" -Destination "$target/test-root/root/prototype.py"
    & "$build/bin/Release/PackMaker.exe" --input "$target/test-root/root" --output "$target/pack"
    if ($LASTEXITCODE -ne 0) { throw 'Private fixture packaging failed.' }
}
$hash = (Get-FileHash -LiteralPath $binary -Algorithm SHA256).Hash
if ((Get-FileHash -LiteralPath "$target/Metin2_Release.exe" -Algorithm SHA256).Hash -ne $hash) { throw 'Binary copy differs.' }
$clientArguments = @('--load-warmup-audit', "--gr2-reader=$Reader", "--animation-runtime=$Reader", "--gr2-prewarm=$Prewarm")
"SourceBinary=$binary`nSHA256=$hash`nArguments=$($clientArguments -join ' ')`nManual=$Manual" | Set-Content -LiteralPath "$target/artifact.txt"
$style = if ($Manual) { 'Normal' } else { 'Hidden' }
$process = Start-Process -FilePath "$target/Metin2_Release.exe" -WorkingDirectory $target -WindowStyle $style -ArgumentList $clientArguments -PassThru
"F2-P PID=$($process.Id) Runtime=$target"
$watch = [Diagnostics.Stopwatch]::StartNew()
while (-not $process.WaitForExit(1000)) {
    if (-not $Manual -and $watch.Elapsed.TotalSeconds -gt 110) {
        Stop-Process -Id $process.Id
        throw 'Owned performance fixture exceeded 110 seconds; evidence retained.'
    }
}
"PID=$($process.Id) ExitCode=$($process.ExitCode) Seconds=$($watch.Elapsed.TotalSeconds)" | Tee-Object -FilePath "$target/exit.txt"
if ($process.ExitCode -ne 0) { throw 'Performance client failed.' }
$audit = Get-Content -LiteralPath "$target/source-resource-audit.log" -Raw
foreach ($field in @('GR2ReaderResources','AssetDocuments','AnimationInstances','MeshBindings','RuntimeSkeletons','RuntimeAnimationClips',
    'IndependentAnimationInstances','AnimationRuntimeFailures','AllCPUDeformationCalls','GPUFallbacks','SkinPreparationFailures',
    'SourceTextures','SourceBuffers','SkinMeshes','BoneRemaps','BonePalettes','PrototypeGeometry','PrototypePalettes','StaticSkinMeshes')) {
    if ($audit -notmatch "\b$field=0\b") { throw "Nonzero/missing shutdown field: $field" }
}
if ($Reader -eq 'ziinan' -and ($audit -notmatch '\bGrannyFileReads=0\b' -or $audit -notmatch '\bNativeGR2Reads=[1-9]')) { throw 'Native route not proved.' }
if ($Reader -eq 'granny' -and ($audit -notmatch '\bNativeGR2Reads=0\b' -or $audit -notmatch '\bGrannyFileReads=[1-9]')) { throw 'Granny route not proved.' }
if ($Reader -eq 'ziinan' -and $Prewarm -eq 'on') {
    foreach ($field in @('NativePrewarmFailures','NativePrewarmLimited','NativeBoundClipBypasses')) {
        if ($audit -notmatch "\b$field=0\b") { throw "Incomplete prewarm: $field; inspect buffered diagnostic and resource logs." }
    }
    if ($audit -notmatch '\bNativePrewarmRequests=[1-9]') { throw 'No native prewarm requests captured.' }
}
$summary = Get-Content -LiteralPath "$target/animation-stall-summary.txt" -Raw
if ($summary -notmatch 'FullFrameCapture=1' -or $summary -notmatch 'DroppedFrames=0\b') { throw 'Full, undropped frame capture required.' }
if (-not $Manual) {
    $fixture = Get-Content -LiteralPath "$target/load-warmup-fixture.json" -Raw | ConvertFrom-Json
    if (-not $fixture.complete) { throw 'Fixture did not complete.' }
    foreach ($phase in 1..6) { if ($fixture.frames[$phase] -lt 100) { throw "Insufficient phase $phase frames." } }
    if ($Reader -eq 'ziinan' -and $Prewarm -eq 'on') {
        $frames = Import-Csv -LiteralPath "$target/load-warmup-frames.csv"
        foreach ($frame in $frames) {
            if ([int]$frame.phase -lt 1 -or [int]$frame.phase -gt 6) { continue }
            foreach ($job in @('gr2_read','decompress','container','parse','animation_decode','clip_bind','skeleton','mesh','material','fingerprint')) {
                if ([long]$frame.("${job}_calls") -ne 0) { throw "Gameplay asset work in phase $($frame.phase): $job" }
            }
        }
    }
}
Write-Output $audit
Write-Output $summary
