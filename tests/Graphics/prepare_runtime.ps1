param(
    [Parameter(Mandatory=$true)][ValidatePattern('^[a-zA-Z0-9_-]+$')][string]$Name,
    [string]$BuildDirectory='build-hx-clean',
    [switch]$Manual
)
$ErrorActionPreference='Stop'
$source=(Resolve-Path -LiteralPath "$PSScriptRoot/../..").Path
$original=(Resolve-Path -LiteralPath "$source/../m2dev-client").Path
$build=(Resolve-Path -LiteralPath $BuildDirectory).Path
$target=Join-Path $source "build/g0x/runtime/$Name"
if(Test-Path -LiteralPath $target) {throw 'Fresh G0-X evidence directory required.'}
New-Item -ItemType Directory -Path "$target/test-root","$target/pack","$target/log","$target/mark","$target/upload" | Out-Null
Copy-Item -LiteralPath "$build/bin/Release/Metin2_Release.exe" -Destination "$target/Metin2_Release.exe"
Copy-Item -LiteralPath "$original/config" -Destination "$target/config" -Recurse
Copy-Item -LiteralPath "$original/assets/root" -Destination "$target/test-root/root" -Recurse
Copy-Item -LiteralPath "$source/build/hx/compiled/vegetation" -Destination "$target/vegetation" -Recurse
if(-not $Manual) {
    $entry=[IO.File]::ReadAllText("$PSScriptRoot/runtime_entry.py")
    $indented=($entry -split "`n" | ForEach-Object {"    $_"}) -join "`n"
    $wrapped="import builtins, traceback`ntry:`n$indented`nexcept Exception:`n    with builtins.old_open('g0x-failure.log', 'w') as failure:`n        failure.write(traceback.format_exc())`n    import app`n    app.Exit()`n"
    [IO.File]::WriteAllText("$target/test-root/root/prototype.py", $wrapped, [Text.UTF8Encoding]::new($false))
}
foreach($package in Get-ChildItem -LiteralPath "$original/pack" -File) {
    if($package.Name -ne 'root.pck') {New-Item -ItemType HardLink -Path "$target/pack/$($package.Name)" -Target $package.FullName | Out-Null}
}
New-Item -ItemType Junction -Path "$target/bgm" -Target "$original/bgm" | Out-Null
& "$build/bin/Release/PackMaker.exe" --input "$target/test-root/root" --output "$target/pack" *> "$target/package.log"
if($LASTEXITCODE -ne 0) {throw 'Private G0-X root packaging failed.'}
$hash=(Get-FileHash -LiteralPath "$target/Metin2_Release.exe" -Algorithm SHA256).Hash
"SourceBinary=$build/bin/Release/Metin2_Release.exe`nSHA256=$hash`nArguments=--renderer-diagnostics`nManual=$Manual" | Set-Content -LiteralPath "$target/artifact.txt"
Write-Output $target
