param([Parameter(Mandatory=$true)][string]$RuntimeDirectory,[switch]$WithoutOverride,[switch]$VegetationProof)
$ErrorActionPreference='Stop'
$target=(Resolve-Path -LiteralPath $RuntimeDirectory).Path
$runs=if($VegetationProof){@(0)}else{@(0,1,2)}
foreach($run in $runs) {
    $process=Start-Process -FilePath "$target/Metin2_Release.exe" -WorkingDirectory $target -WindowStyle Hidden -ArgumentList '--renderer-diagnostics' -PassThru
    $watch=[Diagnostics.Stopwatch]::StartNew()
    while(-not $process.WaitForExit(1000)) {
        if($watch.Elapsed.TotalSeconds -gt 65) {Stop-Process -Id $process.Id;throw 'Owned G2 fixture exceeded bounded deadline; evidence retained.'}
    }
    $process.WaitForExit()
    "Run=$run ExitCode=$($process.ExitCode) Seconds=$($watch.Elapsed.TotalSeconds)" | Add-Content -LiteralPath "$target/exit.txt"
    if($process.ExitCode -ne 0) {throw 'G2 native client returned nonzero exit code.'}
    if(Test-Path -LiteralPath "$target/g2x-failure.log") {throw (Get-Content -LiteralPath "$target/g2x-failure.log" -Raw)}
    $expected=if($run -eq 0 -and -not $VegetationProof){19}else{2}
    $log=Get-Content -LiteralPath "$target/g2x-run-$run.log" -Raw
    if($log -notmatch "PASS steps=$expected frames=[1-9][0-9]* screenshots=$expected restart=$run") {throw 'G2 native sequence incomplete.'}
    $audit=Get-Content -LiteralPath "$target/source-resource-audit.log" -Raw
    foreach($field in @('SceneLightingResources','LightBuffers','LightingPipelines','GraphicsSettingsObjects','MaterialRuntimeObjects','PBRBindings','PBRPipelines','SourceTextures','SourceBuffers','AssetDocuments','GR2ReaderResources','VegetationAssets','VegetationInstances','VegetationRenderAssets','VegetationGeometry','VegetationFailures','AllCPUDeformationCalls','GPUFallbacks','SkinPreparationFailures','CollisionResources')) {
        if($audit -notmatch "\b$field=0\b") {throw "Expected $field=0 at shutdown"}
    }
    foreach($field in @('NativeGR2Reads','GPUFrames','VegetationDraws')) {
        if($audit -notmatch "\b$field=[1-9][0-9]*\b") {throw "Coverage missing: $field"}
    }
    if($run -lt 2) {
        foreach($field in @('PBRDraws','ModernTerrainDraws','ModernVegetationDraws')) {
            if($audit -notmatch "\b$field=[1-9][0-9]*\b") {throw "Modern coverage missing: $field"}
        }
    }
    if($WithoutOverride -and $audit -notmatch '\bMaterialOverrides=0\b') {throw 'Diffuse-only baseline unexpectedly used an override.'}
    if(-not $WithoutOverride -and $audit -notmatch '\bMaterialOverrides=[1-9][0-9]*\b') {throw 'GR2 sidecar never resolved.'}
    Copy-Item -LiteralPath "$target/source-resource-audit.log" -Destination "$target/source-resource-audit-$run.log"
    $renderer=Get-Content -LiteralPath "$target/terrain-renderer.log" | Where-Object {$_ -match '^shutdown '}
    if($renderer.Count -ne 8 -or ($renderer -join ' ') -match '=[1-9][0-9]*') {throw 'Renderer shutdown counters incomplete or nonzero.'}
    Copy-Item -LiteralPath "$target/terrain-renderer.log" -Destination "$target/terrain-renderer-$run.log"
    $errors=Get-Content -LiteralPath "$target/log/syserr.txt" -Raw
    if($errors -match '(?i)Asset Runtime[^\r\n]*(failed|error=)|Optional material map|Material override rejected|Traceback|Vegetation Runtime') {throw 'Unexpected asset/material/vegetation failure.'}
    Copy-Item -LiteralPath "$target/log/syserr.txt" -Destination "$target/syserr-$run.txt"
    if(Test-Path -LiteralPath "$target/skinning-benchmark.csv") {Copy-Item -LiteralPath "$target/skinning-benchmark.csv" -Destination "$target/benchmark-$run.csv"}
}
$log=Get-Content -LiteralPath "$target/g2x-run-0.log" -Raw
if(-not $VegetationProof) {
    foreach($style in @('Classic','Modern')) {if($log -notmatch "window style=$style result=") {throw "Native window check missing: $style"}}
}
Get-Content -LiteralPath "$target/exit.txt"
Write-Output "PASS G2 native lighting/A-B/UI/style/restarts/map/lifetime proof: $target"
