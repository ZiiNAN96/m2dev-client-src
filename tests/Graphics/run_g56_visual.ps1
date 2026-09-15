param([Parameter(Mandatory=$true)][string]$RuntimeDirectory,[int[]]$Views=(0..6))
$ErrorActionPreference='Stop'
$target=(Resolve-Path -LiteralPath $RuntimeDirectory).Path
$process=Start-Process -FilePath "$target/Metin2_Release.exe" -WorkingDirectory $target -WindowStyle Hidden -ArgumentList '--renderer-diagnostics' -PassThru
$watch=[Diagnostics.Stopwatch]::StartNew()
while(-not $process.WaitForExit(1000)) {
    if($watch.Elapsed.TotalSeconds -gt 85){Stop-Process -Id $process.Id;throw 'G56 fixed view deadline exceeded.'}
}
$process.WaitForExit()
"ExitCode=$($process.ExitCode) Seconds=$($watch.Elapsed.TotalSeconds)" | Set-Content -LiteralPath "$target/exit.txt"
if($process.ExitCode -ne 0){throw 'G56 visual client failed.'}
if(Test-Path -LiteralPath "$target/g56-visual-failure.log"){throw (Get-Content -LiteralPath "$target/g56-visual-failure.log" -Raw)}
$log=Get-Content -LiteralPath "$target/asset-runtime-smoke.log" -Raw
if($log -notmatch 'completed phases=[1-9] frames=[1-9]'){throw 'Fixed view sequence did not complete.'}
foreach($phase in $Views){if($log -notmatch "screenshot phase=$phase camera=.+ success=1"){throw "Missing view $phase"}}
$audit=Get-Content -LiteralPath "$target/source-resource-audit.log" -Raw
foreach($key in @('AllCPUDeformationCalls','GPUFallbacks','SourceTextures','SourceBuffers','GR2ReaderResources','RuntimeSkeletons','RuntimeAnimationClips','VegetationAssets','VegetationFailures')){
    if($audit -notmatch "\b$key=0\b"){throw "Nonzero $key"}
}
if((Get-Item -LiteralPath "$target/log/syserr.txt").Length -ne 0){throw 'Visual fixture logged a Python/game error.'}
Write-Output "PASS $($Views.Count) fixed world/sun views; zero fallback and retained resources: $target"
