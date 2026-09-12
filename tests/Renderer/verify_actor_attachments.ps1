# ZiiNAN: Diligent actor attachment rendering
param([Parameter(Mandatory=$true)][string]$TestRoot)
$ErrorActionPreference='Stop'
$testRoot=(Resolve-Path -LiteralPath $TestRoot).Path
function Require([bool]$condition,[string]$message) {
    if(-not $condition) { throw $message }
}
foreach($backend in @('legacy','diligent')) {
    $scene=Get-Content -LiteralPath "$testRoot/$backend-runtime/actor-attachments-test.log" -Raw
    $exitResult=Get-Content -LiteralPath "$testRoot/$backend-exit.txt" -Raw
    Require ($exitResult -match 'ExitCode=0\b') "$backend did not exit normally"
    Require ($scene.Contains('normal actor/map/window shutdown')) "$backend teardown incomplete"
    for($phase=0;$phase -lt 26;$phase++) {
        Require ($scene -match "(?m)^phase=$phase ") "$backend missing phase $phase"
    }
    $maps=[regex]::Matches($scene,'map loaded: (a1|b1)') | ForEach-Object { $_.Groups[1].Value }
    Require (($maps -join ',') -eq 'a1,b1,a1') "$backend map sequence incomplete"
    foreach($race in 0..7) {
        $hair=1001+($race%4)*1000
        Require ($scene -match "race=$race weapon=\d+ hair=$hair\b") "$backend missing alternate hair for race $race"
    }
    foreach($weapon in @(0,19,29,1009,2009,3009,5009,7009)) {
        Require ($scene -match "weapon=$weapon ") "$backend missing weapon $weapon"
    }
    # Only the independently observed absent original audio file is permitted; never hide renderer/asset failures.
    $errors=@(Get-Content -LiteralPath "$testRoot/$backend-runtime/log/syserr.txt" | Where-Object {
        $_.Trim() -and $_ -notmatch "Internal_LoadSoundFromPack: SoundEngine: Failed to register file 'sound/pc2/assassin/bow/attack1.wav' - not found\."
    })
    Require ($errors.Count -eq 0) "$backend unexpected errors: $($errors -join [Environment]::NewLine)"
    Write-Output "$backend : 26 phases, 8 races/genders, 6 weapon types, native hair variants, A1/B1/A1, normal teardown, Exit 0"
}
$actors=Get-Content -LiteralPath "$testRoot/diligent-runtime/actor-renderer.log" -Raw
$frames=Get-Content -LiteralPath "$testRoot/diligent-runtime/terrain-renderer.log" -Raw
Require ($actors -notmatch 'excluded:|ERROR:') 'An original actor or attachment material was excluded/failed'
foreach($race in 0..7) {
    Require ($actors -match "submitted: CPU-skinned main body race=$race ") "Missing body race $race"
    Require ($actors -match "submitted: rigid attachment race=$race .*part=1 ") "Missing right weapon race $race"
    Require ($actors -match "submitted: CPU-skinned attachment race=$race .*part=4 ") "Missing hair race $race"
    Require ($actors -match "hair_2_1.dds race=$race .*part=4 ") "Missing alternate hair texture race $race"
}
foreach($race in @(1,5)) {
    Require ($actors -match "submitted: rigid attachment race=$race .*part=3 ") "Missing left weapon race $race"
    Require ($actors -match "weapon_chogeup_02.dds race=$race .*part=3 ") "Missing bow race $race"
}
Require ($actors -match 'alpha_test=2 .*hair_2_1.dds race=0 .*part=4 ') 'Original alpha-test hair not drawn'
Require ($actors -match 'blend=1 .*part=4 ') 'Hair did not inherit native actor fade'
Require ($actors -match 'blend=1 .*part=1 ') 'Weapon did not inherit native actor fade'
Require ($frames.Contains('attachments_visible=18 weapon_draws=10 shield_draws=0 hair_draws=8')) 'Mixed weapon/hair frame missing'
Require ($frames.Contains('attachments_visible=16 weapon_draws=8 shield_draws=0 hair_draws=8')) 'Single-hand/bow set must not retain old hand models'
Require ($frames.Contains('attachments_visible=8 weapon_draws=0 shield_draws=0 hair_draws=8')) 'Unequip did not remove all weapon draws'
Require ($frames.Contains('attachments_visible=0 weapon_draws=0 shield_draws=0 hair_draws=0 attachment_geometry=0 attachment_textures=0')) 'Despawn did not release all attachments'
Require ($frames.Contains('shutdown attachment_geometry=0 attachment_textures=0')) 'Attachment shutdown resources not zero'
Require ($frames.Contains('shutdown actor_geometry=0 actor_textures=0')) 'Actor shutdown resources not zero'
Require ($frames.Contains('shutdown object_geometry=0 object_textures=0')) 'Object shutdown resources not zero'
Write-Output '5C original rigid weapons / skinned hair / alpha / fade / equipment / gender / lifetime: PASS'
Write-Output 'Shield: no visual Legacy path (explicit user decision); no new shield feature.'
