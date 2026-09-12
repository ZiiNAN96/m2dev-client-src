# ZiiNAN: Diligent mount actor rendering; private test folders, original asset packages unchanged.
param([Parameter(Mandatory=$true)][ValidatePattern('^[a-zA-Z0-9_-]+$')][string]$Name,[switch]$Normal)
$ErrorActionPreference='Stop'
$source=(Resolve-Path -LiteralPath "$PSScriptRoot/../..").Path
$original=(Resolve-Path -LiteralPath "$source/../m2dev-client").Path
$target=Join-Path "$source/build/milestone5d" $Name
if(Test-Path -LiteralPath $target) { throw 'Choose a new test folder; never overwrite earlier evidence.' }
New-Item -ItemType Directory -Path $target | Out-Null
$pack="$original/pack"
if(-not $Normal) {
    New-Item -ItemType Directory -Path "$target/test-root","$target/pack" | Out-Null
    Copy-Item -LiteralPath "$source/build/milestone5a/gate-after/test-root/root" -Destination "$target/test-root/root" -Recurse
    Copy-Item -LiteralPath "$PSScriptRoot/mounts_entry.py" -Destination "$target/test-root/root/prototype.py"
    Copy-Item -LiteralPath "$PSScriptRoot/mounts_smoke.py" -Destination "$target/test-root/root/mounts_smoke.py"
    foreach($package in Get-ChildItem -LiteralPath "$original/pack" -File) {
        if($package.Name -ne 'root.pck') { New-Item -ItemType HardLink -Path "$target/pack/$($package.Name)" -Target $package.FullName | Out-Null }
    }
    & "$source/build/bin/Release/PackMaker.exe" --input "$target/test-root/root" --output "$target/pack"
    if($LASTEXITCODE) { throw 'Mount fixture packaging failed.' }
    $pack="$target/pack"
}
foreach($backend in @('legacy','diligent')) {
    New-Item -ItemType Directory -Path "$target/$backend-bin","$target/$backend-runtime" | Out-Null
    Copy-Item -LiteralPath "$source/build/milestone5a/normal-gate/$backend-runtime/config" -Destination "$target/$backend-runtime/config" -Recurse
    New-Item -ItemType Junction -Path "$target/$backend-runtime/pack" -Target $pack | Out-Null
    New-Item -ItemType Junction -Path "$target/$backend-runtime/bgm" -Target "$original/bgm" | Out-Null
    Copy-Item -LiteralPath "$source/build/bin/Release/Metin2_Release.exe" -Destination "$target/$backend-bin/Metin2_Release.exe"
}
Write-Output $target
