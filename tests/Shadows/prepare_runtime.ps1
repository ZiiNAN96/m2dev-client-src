param(
    [Parameter(Mandatory=$true)][ValidatePattern('^[a-zA-Z0-9_-]+$')][string]$Name,
    [string]$BuildDirectory='build-hx-clean',
    [switch]$Manual,
    [switch]$WithoutOverride
)
$ErrorActionPreference='Stop'
$source=(Resolve-Path -LiteralPath "$PSScriptRoot/../..").Path
$original=(Resolve-Path -LiteralPath "$source/../m2dev-client").Path
$build=(Resolve-Path -LiteralPath $BuildDirectory).Path
$target=Join-Path $source "build/g34x/runtime/$Name"
if(Test-Path -LiteralPath $target) {throw 'Fresh G34-X evidence directory required.'}
New-Item -ItemType Directory -Path "$target/test-root","$target/pack","$target/log","$target/mark","$target/upload" | Out-Null
Copy-Item -LiteralPath "$build/bin/Release/Metin2_Release.exe" -Destination "$target/Metin2_Release.exe"
Copy-Item -LiteralPath "$original/config" -Destination "$target/config" -Recurse
Copy-Item -LiteralPath "$original/assets/root" -Destination "$target/test-root/root" -Recurse
Copy-Item -LiteralPath "$source/build/hx/compiled/vegetation" -Destination "$target/vegetation" -Recurse
if(-not $Manual) {
    Copy-Item -LiteralPath "$PSScriptRoot/../Materials/fixtures" -Destination "$target/test-root/root/g1x" -Recurse
    Copy-Item -LiteralPath "$PSScriptRoot/../AssetRuntime/fixtures/f5x" -Destination "$target/test-root/root/f5x" -Recurse
    Copy-Item -LiteralPath "$PSScriptRoot/../Materials/fixtures/character_pbr.glb" -Destination "$target/test-root/root/f5x/character.glb" -Force
    if(-not $WithoutOverride) {
        New-Item -ItemType Directory -Path "$target/test-root/root/ymir work/pc/warrior" -Force | Out-Null
        $gr2="$original/assets/PC/ymir work/pc/warrior/warrior_novice.gr2"
        $before=(Get-FileHash -LiteralPath $gr2 -Algorithm SHA256).Hash
        & "$build/tests/Materials/Release/GR2OverrideTest.exe" $gr2 "$target/test-root/root/ymir work/pc/warrior/warrior_novice.gr2.zmat" *> "$target/override-proof.log"
        if($LASTEXITCODE -ne 0) {throw 'Neutral GR2 override fixture failed.'}
        $after=(Get-FileHash -LiteralPath $gr2 -Algorithm SHA256).Hash
        if($before -ne $after) {throw 'Original GR2 changed.'}
        "OriginalGR2=$gr2`nBefore=$before`nAfter=$after" | Set-Content -LiteralPath "$target/gr2-hash.txt"
    }
    $entry=[IO.File]::ReadAllText("$PSScriptRoot/runtime_entry.py")
    $indented=($entry -split "`n" | ForEach-Object {"    $_"}) -join "`n"
    $wrapped="import builtins, traceback`ntry:`n$indented`nexcept Exception:`n    with builtins.old_open('g34x-failure.log', 'w') as failure:`n        failure.write(traceback.format_exc())`n    import app`n    app.Exit()`n"
    [IO.File]::WriteAllText("$target/test-root/root/prototype.py",$wrapped,[Text.UTF8Encoding]::new($false))
}
foreach($package in Get-ChildItem -LiteralPath "$original/pack" -File) {
    if($package.Name -ne 'root.pck') {New-Item -ItemType HardLink -Path "$target/pack/$($package.Name)" -Target $package.FullName | Out-Null}
}
New-Item -ItemType Junction -Path "$target/bgm" -Target "$original/bgm" | Out-Null
& "$build/bin/Release/PackMaker.exe" --input "$target/test-root/root" --output "$target/pack" *> "$target/package.log"
if($LASTEXITCODE -ne 0) {throw 'Private G34-X root packaging failed.'}
$hash=(Get-FileHash -LiteralPath "$target/Metin2_Release.exe" -Algorithm SHA256).Hash
"SourceBinary=$build/bin/Release/Metin2_Release.exe`nSHA256=$hash`nArguments=--renderer-diagnostics`nManual=$Manual`nWithoutOverride=$WithoutOverride" | Set-Content -LiteralPath "$target/artifact.txt"
Write-Output $target
