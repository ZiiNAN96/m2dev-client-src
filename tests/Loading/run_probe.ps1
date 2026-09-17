param([Parameter(Mandatory=$true)][string]$Name)
$ErrorActionPreference='Stop'
if($Name -notmatch '^[a-zA-Z0-9_-]+$'){throw 'Invalid evidence name'}
$source=(Resolve-Path -LiteralPath "$PSScriptRoot/../..").Path
$target=(Resolve-Path -LiteralPath "$source/build-p0l/$Name").Path
if(Test-Path -LiteralPath "$target/exit.json"){throw 'Fresh run required'}
$env:M2_MAP_LOAD_TRACE='1'
$watch=[Diagnostics.Stopwatch]::StartNew()
$process=Start-Process -FilePath "$target/Metin2_Release.exe" -WorkingDirectory $target -ArgumentList '--renderer-diagnostics' -WindowStyle Hidden -PassThru
$process.Id | Set-Content -LiteralPath "$target/process-id.txt"
while(-not $process.WaitForExit(1000)){
    if($watch.Elapsed.TotalSeconds -gt 240){$process.Kill();throw 'Owned probe timeout'}
}
$process.WaitForExit()
@{ExitCode=$process.ExitCode;Seconds=$watch.Elapsed.TotalSeconds;PID=$process.Id} | ConvertTo-Json | Set-Content -LiteralPath "$target/exit.json"
if($process.ExitCode -ne 0){throw 'Native probe failed'}
if(Test-Path -LiteralPath "$target/p0l-failure.log"){throw (Get-Content -LiteralPath "$target/p0l-failure.log" -Raw)}
Get-Content -LiteralPath "$target/p0l-smoke.log"
