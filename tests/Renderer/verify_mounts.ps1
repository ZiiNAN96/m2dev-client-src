# ZiiNAN: Diligent mount actor rendering; fail on incomplete scenes or retained GPU resources.
param([Parameter(Mandatory=$true)][string]$TestRoot)
$ErrorActionPreference='Stop'
$testRoot=(Resolve-Path -LiteralPath $TestRoot).Path
function Require([bool]$condition,[string]$message) { if(-not $condition) { throw $message } }
foreach($backend in @('legacy','diligent')) {
    $scene=Get-Content -LiteralPath "$testRoot/$backend-runtime/mounts-test.log" -Raw
    $exitResult=Get-Content -LiteralPath "$testRoot/$backend-exit.txt" -Raw
    Require ($exitResult -match 'ExitCode=0\b') "$backend did not exit normally"
    Require ($scene.Contains('normal mount/actor/map/window shutdown')) "$backend teardown incomplete"
    for($phase=0;$phase -lt 33;$phase++) { Require ($scene -match "(?m)^phase=$phase ") "$backend missing phase $phase" }
    $maps=[regex]::Matches($scene,'map loaded: (a1|b1) mounted=(0|1)') | ForEach-Object { "$($_.Groups[1].Value):$($_.Groups[2].Value)" }
    Require (($maps -join ',') -eq 'a1:0,b1:0,a1:0,b1:1,a1:1') "$backend foot/mounted A1/B1/A1 sequence incomplete"
    foreach($race in 0..7) {
        foreach($mount in @(20104,20110,20114,20219,20225)) {
            Require ($scene -match "race=$race mount=$mount ") "$backend missing race $race on mount $mount"
        }
        $hair=1001+($race%4)*1000
        Require ($scene -match "race=$race mount=20104 weapon=\d+ hair=$hair armor=\d+ ") "$backend mounted equipment/hair change missing for $race"
    }
    foreach($label in @('dismount-1','remount-2','dismount-2','remount-3','hidden','show','out-of-view','reenter','despawn','respawn','final-delete')) {
        Require ($scene -match "label=$label ") "$backend lifecycle $label missing"
    }
    $errors=Get-Content -LiteralPath "$testRoot/$backend-runtime/log/syserr.txt" -Raw
    Require ([string]::IsNullOrWhiteSpace($errors)) "$backend unexpected errors: $errors"
    Write-Output "$backend : 33 phases, 8 rider races/genders, 5 mounts, weapons/hair/armor, repeated mounting, A1/B1/A1 foot and mounted, Exit 0"
}
$actors=Get-Content -LiteralPath "$testRoot/diligent-runtime/actor-renderer.log" -Raw
$frames=Get-Content -LiteralPath "$testRoot/diligent-runtime/terrain-renderer.log" -Raw
Require ($actors -notmatch 'excluded:|ERROR:') 'An original rider/mount/attachment draw was rejected'
foreach($mount in @(20104,20110,20114,20219,20225)) {
    Require ($actors -match "submitted: CPU-skinned main body race=$mount .*part=0 category=3 ") "Mount $mount missing from shared actor renderer"
}
foreach($race in 0..7) {
    Require ($actors -match "submitted: CPU-skinned main body race=$race .*part=0 category=4 ") "Rider $race missing"
    Require ($actors -match "submitted: rigid attachment race=$race .*part=1 category=4 ") "Mounted right weapon $race missing"
    Require ($actors -match "submitted: CPU-skinned attachment race=$race .*part=4 category=4 ") "Mounted hair $race missing"
}
foreach($race in @(1,3,5,7)) {
    Require ($actors -match "submitted: rigid attachment race=$race .*part=3 category=4 ") "Native mounted left weapon missing for $race"
}
Require ($actors -match 'blend=1 .*category=3 ') 'Native mount spawn fade not captured'
Require ($frames.Contains('mounts_visible=8 mounted_actors=8 mount_draws=24 mount_uploads=8 mount_geometry=8 mount_textures=24')) 'Three-material horses not uploaded once per native pose'
Require ($frames.Contains('mounts_visible=8 mounted_actors=8 mount_draws=8 mount_uploads=8')) 'Single-mesh mount/rider pairs missing'
Require ($frames.Contains('mounts_visible=0 mounted_actors=0 mount_draws=0 mount_uploads=0 mount_geometry=0 mount_textures=0')) 'Dismount/deletion did not release mount resources'
Require ($frames.Contains('attachments_visible=20 weapon_draws=12 shield_draws=0 hair_draws=8')) 'Mounted fan/daggers did not use both native hand slots'
Require ($frames.Contains('attachments_visible=16 weapon_draws=8 shield_draws=0 hair_draws=8')) 'Weapon change retained an unused hand'
foreach($group in @('actor','attachment','mount','object')) {
    Require ($frames.Contains("shutdown $($group)_geometry=0 $($group)_textures=0")) "Shutdown $group resources not zero"
}
Write-Output '5D: mount ownership, native saddle/rider/attachments, materials, visibility, repeated lifecycle, resource teardown: PASS'
Write-Output 'Real server teleport/relog and manual window checks are separate evidence; this fixture does not claim them.'
