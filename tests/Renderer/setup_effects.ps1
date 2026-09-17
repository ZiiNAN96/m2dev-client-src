# ZiiNAN: Diligent effect rendering integration; private test folders, original asset packages unchanged.
param([Parameter(Mandatory=$true)][ValidatePattern('^[a-zA-Z0-9_-]+$')][string]$Name,[switch]$Normal,[switch]$AnimationOnly,[switch]$StressOnly,[string]$ClientBinary)
$ErrorActionPreference='Stop'
$source=(Resolve-Path -LiteralPath "$PSScriptRoot/../..").Path
$original=(Resolve-Path -LiteralPath "$source/../m2dev-client").Path
if(-not $ClientBinary) { $ClientBinary="$source/build/bin/Release/Metin2_Release.exe" }
$ClientBinary=(Resolve-Path -LiteralPath $ClientBinary).Path
if(($Normal -and ($AnimationOnly -or $StressOnly)) -or ($AnimationOnly -and $StressOnly)) { throw 'Choose exactly one test mode.' }
$target=Join-Path "$source/build/milestone7" $Name
if(Test-Path -LiteralPath $target) { throw 'Choose a new test folder; never overwrite earlier evidence.' }
New-Item -ItemType Directory -Path $target | Out-Null
$pack="$original/pack"
if(-not $Normal) {
    New-Item -ItemType Directory -Path "$target/test-root","$target/pack" | Out-Null
    Copy-Item -LiteralPath "$source/test-data/legacy-renderer/actor-root" -Destination "$target/test-root/root" -Recurse
    Copy-Item -LiteralPath "$PSScriptRoot/effects_entry.py" -Destination "$target/test-root/root/prototype.py"
    Copy-Item -LiteralPath "$PSScriptRoot/effects_smoke.py" -Destination "$target/test-root/root/effects_smoke.py"
    if($AnimationOnly) { Copy-Item -LiteralPath "$PSScriptRoot/effects_animation_entry.py" -Destination "$target/test-root/root/prototype.py" }
    if($StressOnly) { Copy-Item -LiteralPath "$PSScriptRoot/effects_stress_entry.py" -Destination "$target/test-root/root/prototype.py" }
    foreach($package in Get-ChildItem -LiteralPath "$original/pack" -File) {
        if($package.Name -ne 'root.pck') { New-Item -ItemType HardLink -Path "$target/pack/$($package.Name)" -Target $package.FullName | Out-Null }
    }
    & "$source/build/bin/Release/PackMaker.exe" --input "$target/test-root/root" --output "$target/pack"
    if($LASTEXITCODE) { throw 'Effect fixture packaging failed.' }
    $pack="$target/pack"
}
foreach($backend in @('legacy','diligent')) {
    New-Item -ItemType Directory -Path "$target/$backend-bin","$target/$backend-runtime" | Out-Null
    Copy-Item -LiteralPath "$source/test-data/legacy-renderer/$backend-config" -Destination "$target/$backend-runtime/config" -Recurse
    New-Item -ItemType Junction -Path "$target/$backend-runtime/pack" -Target $pack | Out-Null
    New-Item -ItemType Junction -Path "$target/$backend-runtime/bgm" -Target "$original/bgm" | Out-Null
    Copy-Item -LiteralPath $ClientBinary -Destination "$target/$backend-bin/Metin2_Release.exe"
}
Write-Output $target
