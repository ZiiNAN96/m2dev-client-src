param([Parameter(Mandatory=$true)][string]$Name, [switch]$ShaderLifecycle, [switch]$A1Only, [switch]$AnimationSmoke, [switch]$AnimationFirstUse, [switch]$LoadPrewarm, [switch]$RuntimePreparationDetails, [switch]$SerialAnimationLoading, [string]$BuildDirectory='build')
$ErrorActionPreference='Stop'
$source=(Resolve-Path -LiteralPath "$PSScriptRoot/../..").Path
$original=(Resolve-Path -LiteralPath "$source/../m2dev-client").Path
if(-not [IO.Path]::IsPathFullyQualified($BuildDirectory)){$BuildDirectory=Join-Path $source $BuildDirectory}
$build=(Resolve-Path -LiteralPath $BuildDirectory).Path
foreach($binary in @('Metin2_Release.exe','PackMaker.exe')){
    if(-not (Test-Path -LiteralPath "$build/bin/Release/$binary")){throw "Build Release target UserInterface and PackMaker first: $build/bin/Release/$binary"}
}
if($Name -notmatch '^[a-zA-Z0-9_-]+$'){throw 'Invalid evidence name'}
$target=Join-Path $source "build/loading/$Name"
if(Test-Path -LiteralPath $target){throw 'Fresh evidence directory required'}
New-Item -ItemType Directory -Path "$target/test-root","$target/pack","$target/log","$target/mark","$target/upload" | Out-Null
if($RuntimePreparationDetails){New-Item -ItemType File -Path "$target/animation-preparation-trace.enabled" | Out-Null}
Copy-Item -LiteralPath "$build/bin/Release/Metin2_Release.exe" -Destination "$target/Metin2_Release.exe"
Copy-Item -LiteralPath "$original/config" -Destination "$target/config" -Recurse
Copy-Item -LiteralPath "$original/assets/root" -Destination "$target/test-root/root" -Recurse
if($SerialAnimationLoading){
    # Paired serial control: keep the complete L7C readiness policy and disable
    # only the L7 batch call in this private Python source, never in production.
    $motionPath="$target/test-root/root/playersettingmodule.py"
    $motionSource=[IO.File]::ReadAllText($motionPath)
    if(-not $motionSource.Contains('chrmgr.LoadMotionDataBatch(load)')){throw 'Expected L7 loading boundary'}
    [IO.File]::WriteAllText($motionPath,$motionSource.Replace('chrmgr.LoadMotionDataBatch(load)','load()'),[Text.UTF8Encoding]::new($false))
}
$entry=[IO.File]::ReadAllText("$PSScriptRoot/map_load_probe.py")
if($AnimationSmoke){
    $smoke=[IO.File]::ReadAllText("$PSScriptRoot/animation_smoke.py")
    $entry=$entry.Replace('window = World()', $smoke+"`nwindow = AnimationWorld()")
}
if($AnimationFirstUse){
    if($AnimationSmoke){throw 'Choose one animation probe'}
    $smoke=[IO.File]::ReadAllText("$PSScriptRoot/animation_first_use.py")
    if($LoadPrewarm){$smoke=$smoke.Replace('LOAD_PREWARM = False','LOAD_PREWARM = True')}
    $entry=$entry.Replace('window = World()', $smoke+"`nwindow = FirstUseWorld()")
}
if($A1Only -and $ShaderLifecycle){throw 'Choose A1Only or ShaderLifecycle'}
if($A1Only){
    $entry=[regex]::Replace($entry, '(?s)VIEWS = \[.*?\]', "VIEWS = [('A1-cold', 'metin2_map_a1', 13000, 9500)]", 1)
}
if($ShaderLifecycle){
    $entry=$entry.Replace("('A1-warm', 'metin2_map_a1', 13000, 9500),",'')
    $entry=$entry.Replace("('B1-warm', 'metin2_map_b1', 94300, 27100)","('A1-warm', 'metin2_map_a1', 13000, 9500)")
}
$indented=($entry -split "`n" | ForEach-Object {"    $_"}) -join "`n"
$wrapped="import builtins, traceback`ntry:`n$indented`nexcept Exception:`n    with builtins.old_open('p0l-failure.log', 'w') as failure:`n        failure.write(traceback.format_exc())`n    import app`n    app.Exit()`n"
[IO.File]::WriteAllText("$target/test-root/root/prototype.py",$wrapped,[Text.UTF8Encoding]::new($false))
foreach($package in Get-ChildItem -LiteralPath "$original/pack" -File){
    if($package.Name -ne 'root.pck'){New-Item -ItemType HardLink -Path "$target/pack/$($package.Name)" -Target $package.FullName | Out-Null}
}
New-Item -ItemType Junction -Path "$target/bgm" -Target "$original/bgm" | Out-Null
& "$build/bin/Release/PackMaker.exe" --input "$target/test-root/root" --output "$target/pack" *> "$target/package.log"
if($LASTEXITCODE -ne 0){throw 'Private probe packaging failed'}
Get-FileHash -LiteralPath "$target/Metin2_Release.exe","$target/pack/root.pck" | ConvertTo-Json | Set-Content -LiteralPath "$target/hashes.json"
Write-Output $target
