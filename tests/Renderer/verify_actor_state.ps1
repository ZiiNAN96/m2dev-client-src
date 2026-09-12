# ZiiNAN: Ensure deterministic actor material state
param([Parameter(Mandatory=$true)][string]$TestRoot)
$ErrorActionPreference='Stop'
$testRoot=(Resolve-Path -LiteralPath $TestRoot).Path
function Require([bool]$condition,[string]$message) { if(-not $condition) { throw $message } }
$scene=Get-Content -LiteralPath "$testRoot/diligent-runtime/actor-variants-test.log" -Raw
$actors=Get-Content -LiteralPath "$testRoot/diligent-runtime/actor-renderer.log" -Raw
$frames=Get-Content -LiteralPath "$testRoot/diligent-runtime/terrain-renderer.log" -Raw
$errors=Get-Content -LiteralPath "$testRoot/diligent-runtime/log/syserr.txt" -Raw
$result=Get-Content -LiteralPath "$testRoot/diligent-exit.txt" -Raw
Require ($result -match 'ExitCode=0\b') 'NPC state test did not exit normally'
Require ($scene.Contains('normal actor/map/window shutdown')) 'NPC state teardown incomplete'
Require ([string]::IsNullOrWhiteSpace($errors)) 'NPC state error log is not empty'
Require ($actors -notmatch 'excluded:|ERROR:|capture rejected') 'NPC state capture or submission rejected'
for($phase=0;$phase -lt 43;$phase++) { Require ($scene -match "(?m)^phase=$phase ") "Missing NPC state phase $phase" }
$maps=[regex]::Matches($scene,'map loaded: (a1|b1)') | ForEach-Object { $_.Groups[1].Value }
Require (($maps -join ',') -eq 'a1,b1,a1') 'NPC state map sequence incomplete'
foreach($race in @(20018,20095)) {
    Require ($actors -match "submitted: CPU-skinned main body race=$race ") "Missing NPC $race"
    Require ($actors -match "stage=0 alpha_test=0 blend=1 .*race=$race ") "Missing fade for NPC $race"
    Require ($actors -match "stage=0 alpha_test=0 blend=0 .*race=$race ") "Missing restored opaque material for NPC $race"
    Require ($actors -match "stage=1 .*race=$race ") "Missing ADD material for NPC $race"
}
Require ($frames.Contains('players_visible=1 npcs_visible=4 mobs_visible=2')) 'Mixed NPC/player/mob draw coverage missing'
Require ($frames.Contains('actors_visible=0 actor_uploads=0 actor_vertices=0 actor_bytes=0 actor_draws=0 actor_geometry=0 actor_textures=0')) 'NPC despawn resources not zero'
Require ($frames.Contains('shutdown actor_geometry=0 actor_textures=0')) 'Actor shutdown resources not zero'
Require ($frames.Contains('shutdown object_geometry=0 object_textures=0')) 'Object shutdown resources not zero'
Write-Output 'Doctor/Sinseon: 43 phases, native fade/opaque/ADD, mixed groups, visibility transitions, A1/B1/A1, no refusal/error, zero resources, Exit 0: PASS'
