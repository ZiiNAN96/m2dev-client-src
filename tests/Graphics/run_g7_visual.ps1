param([Parameter(Mandatory=$true)][string]$RuntimeDirectory,[int[]]$Views=(0..17))
$ErrorActionPreference='Stop'
$target=(Resolve-Path -LiteralPath $RuntimeDirectory).Path
$process=Start-Process -FilePath "$target/Metin2_Release.exe" -WorkingDirectory $target -WindowStyle Hidden -ArgumentList '--renderer-diagnostics' -PassThru
$watch=[Diagnostics.Stopwatch]::StartNew()
try {
    while(-not $process.WaitForExit(1000)) {
        if($watch.Elapsed.TotalSeconds -gt 160){throw 'G7 fixed camera deadline exceeded.'}
    }
    $process.WaitForExit()
} finally {
    if(-not $process.HasExited){Stop-Process -Id $process.Id; $process.WaitForExit(5000) | Out-Null}
}
"ExitCode=$($process.ExitCode) Seconds=$($watch.Elapsed.TotalSeconds)" | Set-Content -LiteralPath "$target/exit.txt"
if($process.ExitCode -ne 0){throw 'G7 fixed camera client failed.'}
if(Test-Path -LiteralPath "$target/g7-visual-failure.log"){throw (Get-Content -LiteralPath "$target/g7-visual-failure.log" -Raw)}
$log=Get-Content -LiteralPath "$target/asset-runtime-smoke.log" -Raw
foreach($view in $Views){if($log -notmatch "screenshot phase=$view camera=.+ success=1"){throw "Missing fixed view $view"}}
$audit=Get-Content -LiteralPath "$target/source-resource-audit.log" -Raw
foreach($key in @('AllCPUDeformationCalls','GPUFallbacks','SourceTextures','SourceBuffers','GR2ReaderResources','RuntimeSkeletons','RuntimeAnimationClips','VegetationAssets','VegetationFailures','DiligentErrors','DiligentFatals')){
    if($audit -notmatch "\b$key=0\b"){throw "Nonzero $key"}
}
$renderer=Get-Content -LiteralPath "$target/gdx-renderer.log" -Raw
if($renderer -notmatch 'waterFrames=[1-9]' -or $renderer -notmatch 'ssrFrames=[1-9]' -or $renderer -notmatch '\bssrFallbacks=0\b' -or $renderer -notmatch '\bWaterRenderers=0\b' -or $renderer -notmatch '\bModernRenderers=0\b'){throw 'Missing successful water/SSR/shutdown counters.'}
if(Test-Path -LiteralPath "$target/diligent-diagnostics.log"){
    if((Get-Item -LiteralPath "$target/diligent-diagnostics.log").Length -ne 0){throw 'Diligent warning/error diagnostics require review.'}
}
if((Get-Item -LiteralPath "$target/log/syserr.txt").Length -ne 0){throw 'G7 fixture logged a Python/game error.'}
"PASS $($Views.Count) fixed water views; zero fallback and retained resources: $target"
