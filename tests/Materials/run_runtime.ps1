param([Parameter(Mandatory=$true)][string]$RuntimeDirectory,[switch]$WithoutOverride)
$ErrorActionPreference='Stop'
$target=(Resolve-Path -LiteralPath $RuntimeDirectory).Path
foreach($run in 1..2) {
    $process=Start-Process -FilePath "$target/Metin2_Release.exe" -WorkingDirectory $target -WindowStyle Hidden -ArgumentList '--renderer-diagnostics' -PassThru
    $watch=[Diagnostics.Stopwatch]::StartNew()
    while(-not $process.WaitForExit(1000)) {
        if($watch.Elapsed.TotalSeconds -gt 65) {Stop-Process -Id $process.Id;throw 'Owned material fixture exceeded short deadline; evidence retained.'}
    }
    $process.WaitForExit()
    "Run=$run ExitCode=$($process.ExitCode) Seconds=$($watch.Elapsed.TotalSeconds)" | Add-Content -LiteralPath "$target/exit.txt"
    if($process.ExitCode -ne 0) {throw 'Native client returned nonzero exit code.'}
    if(Test-Path -LiteralPath "$target/g1x-failure.log") {throw (Get-Content -LiteralPath "$target/g1x-failure.log" -Raw)}
    $logName=if($run -eq 1){'g1x-materials.log'}else{'g1x-restart.log'}
    $expected=if($run -eq 1){12}else{2}
    $log=Get-Content -LiteralPath "$target/$logName" -Raw
    if($log -notmatch "PASS steps=$expected frames=[1-9][0-9]* screenshots=$expected restart=") {throw 'Material/UI sequence incomplete.'}
    $audit=Get-Content -LiteralPath "$target/source-resource-audit.log" -Raw
    foreach($field in @('GraphicsSettingsObjects','MaterialRuntimeObjects','PBRBindings','PBRPipelines','SourceTextures','SourceBuffers','AssetDocuments','GR2ReaderResources','VegetationAssets','VegetationInstances','VegetationRenderAssets','VegetationGeometry','VegetationFailures','AllCPUDeformationCalls','GPUFallbacks','SkinPreparationFailures','CollisionResources')) {
        if($audit -notmatch "\b$field=0\b") {throw "Expected $field=0 at shutdown"}
    }
    foreach($field in @('PBRDraws','NativeGR2Reads','GPUFrames','VegetationDraws')) {
        if($audit -notmatch "\b$field=[1-9][0-9]*\b") {throw "Coverage missing: $field"}
    }
    if(-not $WithoutOverride -and $audit -notmatch '\bMaterialOverrides=[1-9][0-9]*\b') {throw 'Production GR2 sidecar never resolved.'}
    if($WithoutOverride -and $audit -notmatch '\bMaterialOverrides=0\b') {throw 'Baseline unexpectedly used an override.'}
    Copy-Item -LiteralPath "$target/source-resource-audit.log" -Destination "$target/source-resource-audit-$run.log"
    $renderer=Get-Content -LiteralPath "$target/terrain-renderer.log" | Where-Object {$_ -match '^shutdown '}
    if($renderer.Count -ne 8 -or ($renderer -join ' ') -match '=[1-9][0-9]*') {throw 'Renderer shutdown counters incomplete or nonzero.'}
    Copy-Item -LiteralPath "$target/terrain-renderer.log" -Destination "$target/terrain-renderer-$run.log"
    $errors=Get-Content -LiteralPath "$target/log/syserr.txt" -Raw
    if($errors -match '(?i)Asset Runtime[^\r\n]*(failed|error=)|Optional material map|Material override rejected') {throw 'Unexpected asset/material failure.'}
}
$log=Get-Content -LiteralPath "$target/g1x-materials.log" -Raw
foreach($style in @('Classic','Modern')) {if($log -notmatch "window style=$style result=") {throw "Native window check missing: $style"}}
Get-Content -LiteralPath "$target/exit.txt"
Write-Output "PASS native material/UI/style/restart/map/lifetime proof: $target"
