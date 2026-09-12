# ZiiNAN: Assert the real-map mount reproducer and corrected body-only submission from logs.
param([Parameter(Mandatory=$true)][string]$TestRoot,[switch]$ExpectOldMountLeak)
$ErrorActionPreference='Stop'
$root=(Resolve-Path -LiteralPath $TestRoot).Path
$actors=Get-Content -LiteralPath "$root\diligent-runtime\actor-renderer.log" -Raw
$frames=Get-Content -LiteralPath "$root\diligent-runtime\terrain-renderer.log" -Raw
$lifecycle=Get-Content -LiteralPath "$root\diligent-runtime\animated-actor-test.log" -Raw
$exitText=Get-Content -LiteralPath "$root\diligent-exit.txt" -Raw
if($exitText -notmatch 'ExitCode=0' -or $lifecycle -notmatch 'normal actor/map/window shutdown') { throw 'Clean process/lifecycle shutdown missing.' }
if($frames -notmatch 'shutdown actor_geometry=0 actor_textures=0') { throw 'Actor GPU resources remain at shutdown.' }
if((Get-Item -LiteralPath "$root\diligent-runtime\log\syserr.txt").Length) { throw 'Isolated client error log is not empty.' }
if($lifecycle -notmatch 'mount=20114' -or $lifecycle -notmatch 'label=unmounted-return') { throw 'Native mount/body fixture did not run.' }
$leaked=$actors -match 'submitted:.*race=20114'
if($ExpectOldMountLeak) {
    if(-not $leaked -or $frames -notmatch 'actor_vertices=4453 actor_bytes=142496') { throw 'Old mount regression was not reproduced.' }
    Write-Output 'PASS: old build reproduces PC-typed white-lion submission (4453 vertices / 142496 bytes).'
} else {
    if($leaked -or $frames -match 'actor_vertices=4453') { throw 'Mount was submitted by corrected body-only path.' }
    $submitted=[regex]::Matches($actors,'submitted:.*race=0 ')
    if($submitted.Count -ne 4) { throw "Expected four body lifetimes, got $($submitted.Count)." }
    $indexCounts=@([regex]::Matches($frames,'actor_index_uploads=(\d+)') | ForEach-Object { [int]$_.Groups[1].Value })
    if(($indexCounts | Measure-Object -Maximum).Maximum -ne 4) { throw 'Unexpected body index uploads.' }
    if($frames -notmatch 'actors_visible=0 actor_uploads=0 actor_vertices=0 actor_bytes=0 actor_draws=0 actor_geometry=0 actor_textures=0 actor_index_uploads=1') { throw 'Mounted interval retained actor GPU resources.' }
    if($frames -notmatch 'actors_visible=1 actor_uploads=1 actor_vertices=2207 actor_bytes=70624 actor_draws=3') { throw 'Normal warrior body did not resume.' }
    if(([regex]::Matches($lifecycle,'map loaded:')).Count -ne 3) { throw 'A1/B1/A1 lifecycle missing.' }
    Write-Output 'PASS: mount excluded, four normal body lifetimes, fixed indices reused, A1/B1/A1, zero actor resources, exit 0.'
}
