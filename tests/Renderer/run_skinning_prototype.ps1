# ZiiNAN: Diligent GPU skinning prototype; one fresh private runtime per test.
param([Parameter(Mandatory=$true)][ValidatePattern('^[a-zA-Z0-9_-]+$')][string]$Name,
      [ValidateSet('cpu','gpu-prototype')][string]$Skinning='cpu',
      [ValidateSet('Release','Debug')][string]$Configuration='Release',
      [switch]$OriginalWorld, [switch]$Visible)
$ErrorActionPreference='Stop'
$source=(Resolve-Path -LiteralPath "$PSScriptRoot/../..").Path
$original=(Resolve-Path -LiteralPath "$source/../m2dev-client").Path
$target="$source/build/phase-b3/$Name"
if(Test-Path -LiteralPath $target) { throw 'Fresh evidence directory required.' }
New-Item -ItemType Directory -Path $target | Out-Null
Copy-Item -LiteralPath "$source/build/bin/$Configuration/Metin2_$Configuration.exe" -Destination "$target/Metin2_Release.exe"
Copy-Item -LiteralPath "$source/build/milestone13c/world-release-final/default-runtime/config" -Destination "$target/config" -Recurse
if($OriginalWorld) {
    New-Item -ItemType Junction -Path "$target/pack" -Target "$source/build/milestone13c/world-release-final/pack" | Out-Null
} else {
    New-Item -ItemType Directory -Path "$target/test-root","$target/pack" | Out-Null
    Copy-Item -LiteralPath "$source/build/milestone13c/world-release-final/test-root/root" -Destination "$target/test-root/root" -Recurse
    Copy-Item -LiteralPath "$PSScriptRoot/gpu_skinning_entry.py" -Destination "$target/test-root/root/prototype.py"
    foreach($package in Get-ChildItem -LiteralPath "$original/pack" -File) {
        if($package.Name -ne 'root.pck') { New-Item -ItemType HardLink -Path "$target/pack/$($package.Name)" -Target $package.FullName | Out-Null }
    }
    & "$source/build/bin/Release/PackMaker.exe" --input "$target/test-root/root" --output "$target/pack"
    if($LASTEXITCODE) { throw 'Private fixture packaging failed.' }
}
New-Item -ItemType Junction -Path "$target/bgm" -Target "$original/bgm" | Out-Null
$arguments=@("--skinning=$Skinning")
if($Skinning -eq 'cpu') { $arguments=@() }
$style=if($Visible) { 'Normal' } else { 'Hidden' }
$start=@{FilePath="$target/Metin2_Release.exe";WorkingDirectory=$target;WindowStyle=$style;PassThru=$true}
if($arguments.Count) { $start.ArgumentList=$arguments }
$process=Start-Process @start
Write-Output "B3 private $Skinning $Configuration test PID=$($process.Id)"
$watch=[System.Diagnostics.Stopwatch]::StartNew()
while(-not $process.HasExited) {
    $process.Refresh()
    if(-not $process.HasExited) {
        [pscustomobject]@{seconds=[math]::Round($watch.Elapsed.TotalSeconds,1);privateMB=[math]::Round($process.PrivateMemorySize64/1MB,1);workingMB=[math]::Round($process.WorkingSet64/1MB,1);handles=$process.HandleCount} |
            Export-Csv -LiteralPath "$target/resources.csv" -NoTypeInformation -Append
    }
    if($process.WaitForExit(5000)) { break }
}
$process.WaitForExit()
"PID=$($process.Id) ExitCode=$($process.ExitCode) Seconds=$([math]::Round($watch.Elapsed.TotalSeconds,1))" |
    Tee-Object -FilePath "$target/exit.txt"
if($process.ExitCode -ne 0) { throw 'World test process failed.' }
$phaseLog=if($OriginalWorld) { 'special-world-test.log' } else { 'gpu-skinning-test.log' }
if(!(Get-Content -LiteralPath "$target/$phaseLog" -Raw).Contains('completed phases=6')) { throw 'World sequence incomplete.' }
$audit=Get-Content -LiteralPath "$target/source-resource-audit.log" -Raw
if(!$audit.Contains('SkinMeshes=0 BoneRemaps=0 BonePalettes=0') -or !$audit.Contains('SkinPreparationFailures=0') -or
   !$audit.Contains('PrototypeGeometry=0 PrototypePalettes=0')) { throw 'Skinning ownership/preparation check failed.' }
if($Skinning -eq 'gpu-prototype' -and !$OriginalWorld -and $audit -notmatch 'GPUFrames=[1-9][0-9]*') { throw 'Prototype never rendered; test invalid.' }
Write-Output $audit
Write-Output 'B3 world test PASS'
