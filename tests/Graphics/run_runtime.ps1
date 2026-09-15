param([Parameter(Mandatory=$true)][string]$RuntimeDirectory)
$ErrorActionPreference='Stop'
$target=(Resolve-Path -LiteralPath $RuntimeDirectory).Path
foreach($run in 1..2) {
    $process=Start-Process -FilePath "$target/Metin2_Release.exe" -WorkingDirectory $target -WindowStyle Hidden -ArgumentList '--renderer-diagnostics' -PassThru
    $watch=[Diagnostics.Stopwatch]::StartNew()
    while(-not $process.WaitForExit(1000)) {
        if($watch.Elapsed.TotalSeconds -gt 55) {throw "Owned G0-X runtime exceeded short deadline; PID=$($process.Id)"}
    }
    $process.WaitForExit()
    "Run=$run ExitCode=$($process.ExitCode) Seconds=$($watch.Elapsed.TotalSeconds)" | Add-Content -LiteralPath "$target/exit.txt"
    if($process.ExitCode -ne 0) {throw 'Native G0-X client returned nonzero exit code.'}
    if(Test-Path -LiteralPath "$target/g0x-failure.log") {throw (Get-Content -LiteralPath "$target/g0x-failure.log" -Raw)}
    $logName=if($run -eq 1){'g0x-settings.log'}else{'g0x-restart.log'}
    $log=Get-Content -LiteralPath "$target/$logName" -Raw
    $expectedSteps=if($run -eq 1){14}else{2}
    if($log -notmatch "PASS steps=$expectedSteps frames=[1-9][0-9]* screenshots=$expectedSteps restart=") {throw 'Settings/UI sequence did not complete.'}
    $audit=Get-Content -LiteralPath "$target/source-resource-audit.log" -Raw
    foreach($field in @('GraphicsSettingsObjects','SourceTextures','SourceBuffers','VegetationAssets','VegetationInstances','VegetationRenderAssets','VegetationGeometry','VegetationFailures','AssetDocuments','CollisionResources')) {
        if($audit -notmatch "\b$field=0\b") {throw "Expected $field=0 on shutdown"}
    }
    Copy-Item -LiteralPath "$target/source-resource-audit.log" -Destination "$target/source-resource-audit-$run.log"
    $renderer=Get-Content -LiteralPath "$target/terrain-renderer.log" | Where-Object {$_ -match '^shutdown '}
    if($renderer.Count -ne 8 -or ($renderer -join ' ') -match '=[1-9][0-9]*') {throw 'Renderer shutdown resource counters are incomplete or nonzero.'}
    Copy-Item -LiteralPath "$target/terrain-renderer.log" -Destination "$target/terrain-renderer-$run.log"
    if((Get-Item -LiteralPath "$target/log/syserr.txt").Length -ne 0) {throw 'Settings runtime error log is not empty.'}
}
$log=Get-Content -LiteralPath "$target/g0x-settings.log" -Raw
foreach($check in @('"minimize": 1','"restore": 1','"resize": 1','"originalSize": 1')) {
    if(-not $log.Contains($check)) {throw "Native window check missing: $check"}
}
Get-Content -LiteralPath "$target/exit.txt"
Write-Output "PASS real client settings/UI/save/restart/resources. Evidence: $target"
