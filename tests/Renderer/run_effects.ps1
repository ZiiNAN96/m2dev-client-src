# ZiiNAN: Diligent effect rendering integration; monitor only this newly started test process.
param([Parameter(Mandatory=$true)][string]$TestRoot,[Parameter(Mandatory=$true)][ValidateSet('default','diligent')][string]$Backend)
$ErrorActionPreference='Stop'
$testRoot=(Resolve-Path -LiteralPath $TestRoot).Path
if(Test-Path -LiteralPath "$testRoot/$Backend-exit.txt") { throw 'Use a fresh test folder.' }
$start=@{FilePath="$testRoot/$Backend-bin/Metin2_Release.exe";WorkingDirectory="$testRoot/$Backend-runtime";PassThru=$true;WindowStyle='Normal'}
if($Backend -eq 'diligent') { $start.ArgumentList='--renderer=d3d11' }
$process=Start-Process @start
Write-Output "Started $Backend PID=$($process.Id)"
$watch=[System.Diagnostics.Stopwatch]::StartNew()
while(-not $process.HasExited) {
    $process.Refresh()
    if(-not $process.HasExited) {
        [pscustomobject]@{seconds=[math]::Round($watch.Elapsed.TotalSeconds,1);privateMB=[math]::Round($process.PrivateMemorySize64/1MB,1);workingMB=[math]::Round($process.WorkingSet64/1MB,1);handles=$process.HandleCount} |
            Export-Csv -LiteralPath "$testRoot/$Backend-resources.csv" -NoTypeInformation -Append
    }
    if($process.WaitForExit(5000)) { break }
}
$process.WaitForExit()
"PID=$($process.Id) ExitCode=$($process.ExitCode) Seconds=$([math]::Round($watch.Elapsed.TotalSeconds,1))" |
    Tee-Object -FilePath "$testRoot/$Backend-exit.txt"
exit $process.ExitCode
