# ZiiNAN: Observe only this explicitly selected test process; never terminate other clients.
param([Parameter(Mandatory=$true)][ValidateSet('legacy','diligent')][string]$Backend,
      [Parameter(Mandatory=$true)][string]$TestRoot,
      [int]$Seconds=1200)
$ErrorActionPreference='Stop'
$testRoot=(Resolve-Path -LiteralPath $TestRoot).Path
$start=@{FilePath="$testRoot\$Backend-bin\Metin2_Release.exe";WorkingDirectory="$testRoot\$Backend-runtime";PassThru=$true;WindowStyle='Normal'}
if($Backend -eq 'diligent') { $start.ArgumentList='--renderer=diligent-d3d11' }
if($Backend -eq 'legacy') { $start.ArgumentList='--renderer=legacy-d3d9' }
$client=Start-Process @start
Write-Output "$Backend PID=$($client.Id)"
$samples=[System.Collections.Generic.List[object]]::new()
$watch=[System.Diagnostics.Stopwatch]::StartNew()
while(-not $client.HasExited -and $watch.Elapsed.TotalSeconds -lt $Seconds) {
    $client.Refresh()
    $samples.Add([pscustomobject]@{seconds=[math]::Round($watch.Elapsed.TotalSeconds,1);privateMB=[math]::Round($client.PrivateMemorySize64/1MB,1);workingMB=[math]::Round($client.WorkingSet64/1MB,1);handles=$client.HandleCount})
    $samples | Export-Csv "$testRoot\$Backend-resources.csv" -NoTypeInformation
    Start-Sleep -Seconds 5
}
if(-not $client.HasExited) { Write-Output 'Test still open; not terminated.'; exit 2 }
$client.WaitForExit()
"PID=$($client.Id) ExitCode=$($client.ExitCode) Seconds=$([math]::Round($watch.Elapsed.TotalSeconds,1))" | Tee-Object -FilePath "$testRoot\$Backend-exit.txt"
exit $client.ExitCode
