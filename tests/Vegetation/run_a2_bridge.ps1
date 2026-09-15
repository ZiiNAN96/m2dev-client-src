param(
    [Parameter(Mandatory=$true)][ValidatePattern('^[a-zA-Z0-9_-]+$')][string]$Name,
    [ValidateSet('ziinan')][string]$Vegetation='ziinan',
    [string]$BuildDirectory='build'
)
$ErrorActionPreference='Stop'
$source=(Resolve-Path -LiteralPath "$PSScriptRoot/../..").Path
$original=(Resolve-Path -LiteralPath "$source/../m2dev-client").Path
$build=(Resolve-Path -LiteralPath $BuildDirectory).Path
$target=Join-Path $source "build/hx/a2/$Name"
if (Test-Path -LiteralPath $target) {throw 'Fresh A2 output required.'}
New-Item -ItemType Directory -Path $target,"$target/pack","$target/log","$target/test-root" | Out-Null
Copy-Item -LiteralPath "$build/bin/Release/Metin2_Release.exe" -Destination "$target/Metin2_Release.exe"
Copy-Item -LiteralPath "$original/config" -Destination "$target/config" -Recurse
Copy-Item -LiteralPath "$original/assets/root" -Destination "$target/test-root/root" -Recurse
Copy-Item -LiteralPath "$PSScriptRoot/a2_bridge_entry.py" -Destination "$target/test-root/root/prototype.py"
if ($Vegetation -eq 'ziinan') {Copy-Item -LiteralPath "$source/test-data/vegetation/vegetation" -Destination "$target/vegetation" -Recurse}
foreach ($file in Get-ChildItem -LiteralPath "$original/pack" -File) {
    if ($file.Name -ne 'root.pck') {New-Item -ItemType HardLink -Path "$target/pack/$($file.Name)" -Target $file.FullName | Out-Null}
}
& "$build/bin/Release/PackMaker.exe" --input "$target/test-root/root" --output "$target/pack"
if ($LASTEXITCODE -ne 0) {throw 'Fixture pack failed.'}
$hash=(Get-FileHash -LiteralPath "$target/Metin2_Release.exe" -Algorithm SHA256).Hash
"SHA256=$hash`nVegetation=$Vegetation`nFixtureSHA256=$((Get-FileHash -LiteralPath "$PSScriptRoot/a2_bridge_entry.py" -Algorithm SHA256).Hash)" | Set-Content -LiteralPath "$target/artifact.txt"
$process=Start-Process -FilePath "$target/Metin2_Release.exe" -WorkingDirectory $target -WindowStyle Hidden -ArgumentList '--renderer-diagnostics' -PassThru
Write-Output "A2 bridge parity PID=$($process.Id) Runtime=$target"
$watch=[Diagnostics.Stopwatch]::StartNew()
while (-not $process.WaitForExit(1000)) {
    if ($watch.Elapsed.TotalSeconds -gt 95) {Stop-Process -Id $process.Id;throw 'Owned A2 fixture exceeded 95 seconds.'}
}
$process.WaitForExit()
"ExitCode=$($process.ExitCode) Seconds=$([math]::Round($watch.Elapsed.TotalSeconds,1))" | Tee-Object -FilePath "$target/exit.txt"
if ($process.ExitCode -ne 0) {throw 'Client failure.'}
$log=Get-Content -LiteralPath "$target/a2-bridge.log" -Raw
if ($log -notmatch 'completed samples=36') {throw 'Camera sequence incomplete.'}
foreach ($match in [regex]::Matches($log,'image=([^\r\n]+)')) {
    if (-not (Test-Path -LiteralPath (Join-Path $target $match.Groups[1].Value))) {throw 'Screenshot missing.'}
}
$audit=Get-Content -LiteralPath "$target/source-resource-audit.log" -Raw
foreach ($field in @('SourceTextures','SourceBuffers','VegetationAssets','VegetationInstances','VegetationRenderAssets','VegetationGeometry','VegetationInstanceBuffers','VegetationFailures','AssetDocuments','MeshBindings','GR2ReaderResources','CollisionResources')) {
    if ($audit -notmatch "\b$field=0\b" -or $audit -match "\b$field=[1-9]") {throw "Resource/failure counter: $field"}
}
if ($Vegetation -eq 'ziinan') {
    foreach ($field in @('VegetationBranches','VegetationFronds','VegetationLeaves','VegetationBillboards','VegetationLODChanges')) {
        if ($audit -notmatch "\b$field=[1-9][0-9]*\b") {throw "Missing coverage: $field"}
    }
    if ((Get-Content -LiteralPath "$target/renderer-startup.log" -Raw) -notmatch 'VegetationSelection=default') {throw 'Production default missing.'}
}
if (Test-Path -LiteralPath "$target/log/syserr.txt") {
    if ((Get-Item -LiteralPath "$target/log/syserr.txt").Length -ne 0) {throw 'Runtime error log is not empty.'}
}
Write-Output $audit
Write-Output 'PASS: 36 fixed A1 bridge cameras, all vegetation parts/LOD, resources 0, exit 0; manual acceptance remains pending.'
