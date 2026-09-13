# ZiiNAN: GPU skinning stability validation
param([Parameter(Mandatory=$true)][string]$RunDirectory)
$ErrorActionPreference='Stop'
$run=(Resolve-Path -LiteralPath $RunDirectory).Path
$exitText=Get-Content -LiteralPath "$run/exit.txt" -Raw
$audit=Get-Content -LiteralPath "$run/source-resource-audit.log" -Raw
$startup=Get-Content -LiteralPath "$run/renderer-startup.log" -Raw
$actors=Get-Content -LiteralPath "$run/actor-renderer.log"
$frames=Get-Content -LiteralPath "$run/terrain-renderer.log"
$errors=Get-Content -LiteralPath "$run/log/syserr.txt"
function Count-Matches([string]$pattern) { return @($errors | Select-String -Pattern $pattern).Count }
function Get-AuditValue([string]$key) {
    $match=[regex]::Match($audit,"\b$([regex]::Escape($key))=([0-9]+)")
    if(!$match.Success) { throw "Missing audit key: $key" }
    return [long]$match.Groups[1].Value
}
function Get-Peak([string]$key) {
    $values=@(foreach($line in $frames) { $match=[regex]::Match($line,"\b$([regex]::Escape($key))=([0-9]+)"); if($match.Success) { [long]$match.Groups[1].Value } })
    if(!$values.Count) { throw "Missing frame metric: $key" }
    return ($values | Measure-Object -Maximum).Maximum
}
$resourceKeys=@('SourceTextures','SourceBuffers','SkinMeshes','BoneRemaps','BonePalettes','PrototypeGeometry','PrototypePalettes','StaticSkinMeshes')
$owners=[ordered]@{};foreach($key in $resourceKeys) { $owners[$key]=Get-AuditValue $key }
$diagnostics=[ordered]@{
    fallbackDiagnostics=Count-Matches 'GPU skinning CPU fallback'
    hairBindingErrors=Count-Matches 'Hair LOD binding refresh failed'
    invalidRemaps=Count-Matches 'InvalidRemap|InvalidBoneIndex'
    staleBindings=Count-Matches 'DestinationChanged|stale binding'
    paletteErrors=Count-Matches 'Skinning palette preparation|InvalidMatrix'
    skinPreparationFailures=Get-AuditValue 'SkinPreparationFailures'
    actorErrors=@($actors | Select-String -Pattern '^ERROR').Count
    guildMarkMessages=Count-Matches 'invalid idx 0'
}
$skins=[ordered]@{gpuFrames=Get-AuditValue 'GPUFrames';cpuReferenceFrames=Get-AuditValue 'CPUReferenceFrames';cpuVertexBytes=Get-AuditValue 'CPUVertexBytes'}
$worlds=[System.Collections.Generic.List[string]]::new()
foreach($line in $actors) {
    if($line -match '^submitted: (GPU-skinned actor part|CPU-skinned main body) race=0 .*?vid=(\d+) part=0 ') {
        $vid=$Matches[2]
        if(!$worlds.Count -or $worlds[$worlds.Count-1] -ne $vid) { $worlds.Add($vid) }
    }
}
$samples=Import-Csv -LiteralPath "$run/resources.csv"
function Number([string]$text) { [double]::Parse($text.Replace(',','.'),[Globalization.CultureInfo]::InvariantCulture) }
$memory=@(foreach($sample in $samples) { Number $sample.privateMB })
$handles=@(foreach($sample in $samples) { Number $sample.handles })
$phases=@();$fixtureLog=Join-Path $run 'gpu-skinning-stability-test.log'
if(Test-Path -LiteralPath $fixtureLog) {
    $fixture=Get-Content -LiteralPath $fixtureLog
    $phases=@($fixture | Select-String -Pattern '^phase=')
    if(!($fixture -match '^completed phases=12$')) { throw 'Stability world sequence incomplete' }
}
$result=[ordered]@{
    run=$run;exit=$exitText.Trim();gpuOptIn=$startup.Contains('Skinning=gpu-prototype');phases=$phases.Count
    playerVidTransitions=$worlds.ToArray();diagnostics=$diagnostics;skinning=$skins;shutdownOwners=$owners
    peaks=[ordered]@{actors=Get-Peak 'actors_visible';npcs=Get-Peak 'npcs_visible';mobs=Get-Peak 'mobs_visible';mounts=Get-Peak 'mounts_visible';geometry=Get-Peak 'actor_geometry';attachments=Get-Peak 'attachment_geometry'}
    processSamples=[ordered]@{count=$samples.Count;privateMBFirst=$memory[0];privateMBLast=$memory[-1];privateMBMax=($memory|Measure-Object -Maximum).Maximum;handlesFirst=$handles[0];handlesLast=$handles[-1];handlesMax=($handles|Measure-Object -Maximum).Maximum}
}
$result | ConvertTo-Json -Depth 6
if($exitText -notmatch '\bExitCode=0\b') { throw 'Nonzero process exit' }
if(@($owners.Values|Where-Object { $_ -ne 0 }).Count) { throw 'Leaked monitored owner' }
foreach($key in @('fallbackDiagnostics','hairBindingErrors','invalidRemaps','staleBindings','paletteErrors','skinPreparationFailures','actorErrors')) {
    if($diagnostics[$key]) { throw "Unexpected skinning diagnostic: $key" }
}
if($result.gpuOptIn -and (!$skins.gpuFrames -or $skins.cpuReferenceFrames -or $skins.cpuVertexBytes)) { throw 'Unexpected GPU inactivity or CPU fallback' }
if(!$result.gpuOptIn -and $skins.gpuFrames) { throw 'CPU test unexpectedly used GPU skinning' }
