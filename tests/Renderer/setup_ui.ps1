# ZiiNAN: Private original-UI fixture. Never replace the user's runtime or packages.
param([Parameter(Mandatory=$true)][ValidatePattern('^[a-zA-Z0-9_-]+$')][string]$Name,[switch]$Normal,[string]$ClientBinary,[string]$UiConfiguration,
    [ValidateSet('milestone9','milestone10a','milestone10b')][string]$Milestone='milestone9')
$ErrorActionPreference='Stop'
$source=(Resolve-Path -LiteralPath "$PSScriptRoot/../..").Path
$original=(Resolve-Path -LiteralPath "$source/../m2dev-client").Path
if(-not $ClientBinary) { $ClientBinary="$source/build/bin/Release/Metin2_Release.exe" }
$ClientBinary=(Resolve-Path -LiteralPath $ClientBinary).Path
$target=Join-Path "$source/build/$Milestone" $Name
if(Test-Path -LiteralPath $target) { throw 'Choose a new test folder; never overwrite earlier evidence.' }
New-Item -ItemType Directory -Path $target | Out-Null
$pack="$original/pack"
if(-not $Normal) {
    New-Item -ItemType Directory -Path "$target/test-root","$target/pack" | Out-Null
    Copy-Item -LiteralPath "$original/assets/root" -Destination "$target/test-root/root" -Recurse
    $entry=if($Milestone -eq 'milestone10b') { 'floating_entry.py' } elseif($Milestone -eq 'milestone10a') { 'text_entry.py' } else { 'ui_entry.py' }
    Copy-Item -LiteralPath "$PSScriptRoot/$entry" -Destination "$target/test-root/root/prototype.py"
    foreach($script in @('water_smoke.py','ui_fixture_panels.py')) {
        Copy-Item -LiteralPath "$PSScriptRoot/$script" -Destination "$target/test-root/root/$script"
    }
    if($Milestone -eq 'milestone10a') {
        Copy-Item -LiteralPath "$PSScriptRoot/text_fixture_panels.py" -Destination "$target/test-root/root/text_fixture_panels.py"
    }
    if($Milestone -eq 'milestone10b') {
        Copy-Item -LiteralPath "$PSScriptRoot/floating_smoke.py" -Destination "$target/test-root/root/floating_smoke.py"
    }
    foreach($package in Get-ChildItem -LiteralPath "$original/pack" -File) {
        if($package.Name -ne 'root.pck') { New-Item -ItemType HardLink -Path "$target/pack/$($package.Name)" -Target $package.FullName | Out-Null }
    }
    & "$source/build/bin/Release/PackMaker.exe" --input "$target/test-root/root" --output "$target/pack"
    if($LASTEXITCODE) { throw 'UI fixture packaging failed.' }
    $pack="$target/pack"
}
foreach($backend in @('legacy','diligent')) {
    New-Item -ItemType Directory -Path "$target/$backend-bin","$target/$backend-runtime" | Out-Null
    Copy-Item -LiteralPath "$source/build/milestone5a/normal-gate/$backend-runtime/config" -Destination "$target/$backend-runtime/config" -Recurse
    if($UiConfiguration) {
        Copy-Item -LiteralPath $UiConfiguration -Destination "$target/$backend-runtime/config/ui-fixture.json"
        Copy-Item -LiteralPath ([System.IO.Path]::ChangeExtension($UiConfiguration,'.cfg')) -Destination "$target/$backend-runtime/config/metin2.cfg"
    }
    New-Item -ItemType Junction -Path "$target/$backend-runtime/pack" -Target $pack | Out-Null
    New-Item -ItemType Junction -Path "$target/$backend-runtime/bgm" -Target "$original/bgm" | Out-Null
    Copy-Item -LiteralPath $ClientBinary -Destination "$target/$backend-bin/Metin2_Release.exe"
}
Write-Output $target
