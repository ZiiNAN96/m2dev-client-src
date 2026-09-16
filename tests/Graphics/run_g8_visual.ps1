param([Parameter(Mandatory=$true)][string]$RuntimeDirectory,[string]$BaselineErrorLog)
$ErrorActionPreference='Stop'
$target=(Resolve-Path -LiteralPath $RuntimeDirectory).Path
$views=Get-Content -LiteralPath "$target/views.json" -Raw | ConvertFrom-Json
$process=Start-Process -FilePath "$target/Metin2_Release.exe" -WorkingDirectory $target -WindowStyle Hidden -ArgumentList '--renderer-diagnostics' -PassThru
$watch=[Diagnostics.Stopwatch]::StartNew()
try {
    while(-not $process.WaitForExit(1000)) {
        if($watch.Elapsed.TotalSeconds -gt (30+16*$views.Count)){throw 'G8 fixed camera deadline exceeded.'}
    }
    $process.WaitForExit()
} finally {
    if(-not $process.HasExited){Stop-Process -Id $process.Id; $process.WaitForExit(5000) | Out-Null}
}
"ExitCode=$($process.ExitCode) Seconds=$($watch.Elapsed.TotalSeconds)" | Set-Content -LiteralPath "$target/exit.txt"
if($process.ExitCode -ne 0){throw 'G8 native client failed.'}
if(Test-Path -LiteralPath "$target/g8-visual-failure.log"){throw (Get-Content -LiteralPath "$target/g8-visual-failure.log" -Raw)}
$log=Get-Content -LiteralPath "$target/g8-visual.log" -Raw
for($view=0;$view -lt $views.Count;$view++) {
    if($log -notmatch "(?m)^capture=$view label="){throw "Missing fixed view $view"}
}
if($log -notmatch "completed=$($views.Count) frames=[1-9]"){throw 'Incomplete G8 series'}
$audit=Get-Content -LiteralPath "$target/source-resource-audit.log" -Raw
foreach($key in @('AllCPUDeformationCalls','GPUFallbacks','SourceTextures','SourceBuffers','GR2ReaderResources','RuntimeSkeletons','RuntimeAnimationClips','VegetationAssets','VegetationFailures','DiligentErrors','DiligentFatals')) {
    if($audit -notmatch "\b$key=0\b"){throw "Nonzero $key"}
}
$renderer=Get-Content -LiteralPath "$target/gdx-renderer.log" -Raw
foreach($line in $renderer -split "`n" | Where-Object {$_ -match 'ModernRenderers='}) {
    foreach($key in @('WaterRenderers','ModernRenderers','ssrFallbacks')) {
        if($line -notmatch "\b$key=0\b"){throw "Nonzero $key"}
    }
}
if((Get-Item -LiteralPath "$target/log/syserr.txt").Length -ne 0){
    if(-not $BaselineErrorLog){throw 'Native scene logged a Python/game error.'}
    $normalize={param($path) @(Get-Content -LiteralPath $path | Where-Object {$_} | ForEach-Object {$_ -replace '^.*? ::\s*',''} | Sort-Object)}
    $known=& $normalize $BaselineErrorLog
    $current=& $normalize "$target/log/syserr.txt"
    if(Compare-Object $known $current){throw 'Native errors differ from the explicitly supplied G7 baseline.'}
    "BASELINE LIMITATION: $($current.Count) game asset messages match G7 exactly; see syserr.txt"
}
if((Test-Path -LiteralPath "$target/diligent-diagnostics.log") -and (Get-Item -LiteralPath "$target/diligent-diagnostics.log").Length -ne 0){throw 'Diligent diagnostics require review.'}
"PASS $($views.Count) native views, exit 0, checked fallback/resource counters 0"
