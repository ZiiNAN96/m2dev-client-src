# ZiiNAN: Diligent SpeedTree rendering integration; evidence checks, no application mutation.
param([Parameter(Mandatory=$true)][string]$TestRoot,[switch]$Normal)
$ErrorActionPreference='Stop'
$testRoot=(Resolve-Path -LiteralPath $TestRoot).Path
foreach($backend in @('legacy','diligent')) {
    $exitText=Get-Content -LiteralPath "$testRoot/$backend-exit.txt" -Raw
    if($exitText -notmatch 'ExitCode=0\b') { throw "$backend did not exit successfully: $exitText" }
    $runtime="$testRoot/$backend-runtime"
    if(Test-Path -LiteralPath "$runtime/log/ErrorLog.txt") { throw "$backend crash report exists" }
    if(-not $Normal) {
        $trace=Get-Content -LiteralPath "$runtime/trees-test.log"
        $maps=@($trace | Where-Object {$_ -match '^map loaded:'})
        if(($maps -join '|') -ne 'map loaded: a1|map loaded: b1|map loaded: a1') { throw "$backend map sequence incomplete" }
        foreach($phase in 0..17) { if(-not ($trace -match "^phase=$phase ")) { throw "$backend phase $phase missing" } }
        if($trace[-1] -ne 'normal tree/actor/map/window shutdown') { throw "$backend fixture shutdown incomplete" }
    }
    Write-Output $exitText.Trim()
    if($backend -eq 'diligent') {
        $treeLog=Get-Content -LiteralPath "$runtime/tree-renderer.log"
        if($treeLog -match '^ERROR:') { throw 'Tree renderer reported an unsupported or failed draw' }
        foreach($part in 0..2) { if(-not ($treeLog -match "^submitted part=$part ")) { throw "Tree component $part not drawn" } }
        $log=Get-Content -LiteralPath "$runtime/terrain-renderer.log"
        foreach($category in @('tree','actor','attachment','mount','object')) {
            if(-not ($log -match "^shutdown ${category}_geometry=0 ${category}_textures=0$")) { throw "$category resources not released" }
        }
        $samples=@($log | Where-Object {$_ -match '^trees_visible='} | ForEach-Object {
            $row=@{}; foreach($match in [regex]::Matches($_,'(\w+)=(\d+)')) { $row[$match.Groups[1].Value]=[long]$match.Groups[2].Value }
            [pscustomobject]$row
        })
        if(-not $samples.Count -or -not ($samples | Where-Object {$_.trees_visible -gt 0})) { throw 'No sampled visible trees' }
        $max=$samples | Sort-Object trees_visible -Descending | Select-Object -First 1
        Write-Output ("Max-visible snapshot: " + ($max | ConvertTo-Json -Compress))
        $assets=@($treeLog | Where-Object {$_ -match '^submitted'} | ForEach-Object {($_ -split 'asset=')[1]} | Sort-Object -Unique)
        Write-Output "Submitted SPT assets: $($assets.Count)"
    }
    $resources=Import-Csv -LiteralPath "$testRoot/$backend-resources.csv"
    $warm=@($resources | Where-Object {[double]($_.seconds.Replace(',','.')) -ge 10})
    if($warm.Count) {
        $memory=$warm | ForEach-Object {[double]($_.privateMB.Replace(',','.'))} | Measure-Object -Minimum -Maximum
        Write-Output "$backend warm private MB min=$($memory.Minimum) max=$($memory.Maximum); last=$($warm[-1].privateMB); handles last=$($warm[-1].handles)"
    }
}
Write-Output 'Tree fixture evidence: PASS (visual/user confirmations documented separately)'
