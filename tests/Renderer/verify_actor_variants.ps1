# ZiiNAN: Fail closed on incomplete original-asset 5B regression evidence.
param([Parameter(Mandatory=$true)][string]$TestRoot)
$ErrorActionPreference='Stop'
$testRoot=(Resolve-Path -LiteralPath $TestRoot).Path
function Require([bool]$condition,[string]$message) {
    if(-not $condition) { throw $message }
}
foreach($backend in @('legacy','diligent')) {
    $scene=Get-Content -LiteralPath "$testRoot/$backend-runtime/actor-variants-test.log" -Raw
    $exitResult=Get-Content -LiteralPath "$testRoot/$backend-exit.txt" -Raw
    Require ($exitResult -match 'ExitCode=0\b') "$backend did not exit normally"
    Require ($scene.Contains('normal actor/map/window shutdown')) "$backend teardown incomplete"
    for($phase=0;$phase -lt 31;$phase++) {
        Require ($scene -match "(?m)^phase=$phase ") "$backend missing phase $phase"
    }
    $maps=[regex]::Matches($scene,'map loaded: (a1|b1)') | ForEach-Object { $_.Groups[1].Value }
    Require (($maps -join ',') -eq 'a1,b1,a1') "$backend map sequence incomplete"
    $errors=Get-Content -LiteralPath "$testRoot/$backend-runtime/log/syserr.txt" -Raw
    Require ([string]::IsNullOrWhiteSpace($errors)) "$backend isolated error log is not empty"
    Write-Output "$backend : 31 phases, A1/B1/A1, normal teardown, Exit 0, empty error log"
}
$actors=Get-Content -LiteralPath "$testRoot/diligent-runtime/actor-renderer.log" -Raw
$frames=Get-Content -LiteralPath "$testRoot/diligent-runtime/terrain-renderer.log" -Raw
Require ($actors -notmatch 'excluded:|ERROR:') 'An original fixture body/material was excluded or failed'
foreach($race in @(0,9003,9002,9004,20016,101,102,110,301,691)) {
    Require ($actors -match "submitted: CPU-skinned main body race=$race ") "Missing race $race"
}
foreach($body in @('warrior_novice','warrior_nahan','warrior_saja','warrior_cheongrin','warrior_4-1')) {
    Require ($actors.Contains("$body.gr2")) "Missing armor model $body"
}
Require ($actors.Contains('warrior_giryung.dds')) 'Missing real shape texture override'
Require ($actors -match 'stage=3 .*warrior_4-1.dds') 'Original armor sphere material not drawn'
Require ($actors -match 'stage=1 .*race=9003 ') 'Original NPC additive material not drawn'
Require ($actors -match 'alpha_test=2 .*race=9002 ') 'NPC alpha-test group not drawn'
Require ($actors -match 'alpha_test=2 .*race=102 ') 'Mob alpha-test group not drawn'
Require ($actors -match 'blend=1 .*race=102 ') 'Mob fade not drawn'
foreach($race in @(20016,301,691)) {
    Require ($actors -match "rigid=1 .*race=$race ") "Missing embedded rigid body piece for $race"
}
Require ($frames.Contains('players_visible=1 npcs_visible=3 mobs_visible=5')) 'Mixed category scene missing'
Require ($frames.Contains('players_visible=4 npcs_visible=12 mobs_visible=20')) '36-actor stress coverage missing'
Require ($frames.Contains('actors_visible=0 actor_uploads=0 actor_vertices=0 actor_bytes=0 actor_draws=0 actor_geometry=0 actor_textures=0')) 'Despawn failed to release actor resources'
Require ($frames.Contains('shutdown actor_geometry=0 actor_textures=0')) 'Actor shutdown resources not zero'
Require ($frames.Contains('shutdown object_geometry=0 object_textures=0')) 'Object shutdown resources not zero'
Write-Output '5B armor / NPC / mob / native materials / embedded rigid geometry / stress / zero GPU lifetime: PASS'
