# ZiiNAN: Rejected selectors run only in a private fixture; no game process is reused.
param([Parameter(Mandatory=$true)][string]$TestRoot,
      [Parameter(Mandatory=$true)][ValidateSet('legacy','invalid')][string]$Case)
$ErrorActionPreference='Stop'
$root=(Resolve-Path -LiteralPath $TestRoot).Path
if(Test-Path -LiteralPath "$root/$Case-exit.txt") { throw 'Use a fresh fixture for each run.' }
$argument=if($Case -eq 'legacy') { '--renderer=legacy-d3d9' } else { '--renderer=invalid-m13b' }
$process=Start-Process -FilePath "$root/default-bin/Metin2_Release.exe" -WorkingDirectory "$root/default-runtime" -ArgumentList $argument -WindowStyle Normal -PassThru
Write-Output "Started expected $Case rejection PID=$($process.Id)"
$process.WaitForExit()
"PID=$($process.Id) ExitCode=$($process.ExitCode)" | Tee-Object -FilePath "$root/$Case-exit.txt"
exit $process.ExitCode
